// CLI:
//   lsm put     <dir> <key> <value>
//   lsm get     <dir> <key>
//   lsm compact <dir>
//   lsm stats   <dir>
//   lsm              (no args: runs the compaction experiment)
use std::env;
use std::process;
use std::time::Instant;

use tiny_lsm::Lsm;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() == 1 {
        // Rust-lab convention (CONTRIBUTING.md): no args -> run the experiment.
        if let Err(e) = run_experiment() {
            eprintln!("error: {e}");
            process::exit(1);
        }
        return;
    }

    let result = match args.get(1).map(String::as_str) {
        Some("put") if args.len() == 5 => run_put(&args[2], &args[3], &args[4]),
        Some("get") if args.len() == 4 => run_get(&args[2], &args[3]),
        Some("compact") if args.len() == 3 => run_compact(&args[2]),
        Some("stats") if args.len() == 3 => run_stats(&args[2]),
        _ => {
            eprintln!(
                "usage:\n  lsm put     <dir> <key> <value>\n  lsm get     <dir> <key>\n  \
                 lsm compact <dir>\n  lsm stats   <dir>"
            );
            process::exit(2);
        }
    };

    if let Err(e) = result {
        eprintln!("error: {e}");
        process::exit(1);
    }
}

fn run_put(dir: &str, key: &str, value: &str) -> std::io::Result<()> {
    let mut lsm = Lsm::open(dir)?;
    lsm.put(key, value)?;
    println!("put {key} = {value}");
    Ok(())
}

fn run_get(dir: &str, key: &str) -> std::io::Result<()> {
    let lsm = Lsm::open(dir)?;
    match lsm.get(key)? {
        Some(v) => println!("{v}"),
        None => println!("(not found)"),
    }
    Ok(())
}

fn run_compact(dir: &str) -> std::io::Result<()> {
    let mut lsm = Lsm::open(dir)?;
    let (before, after) = lsm.compact()?;
    println!("compacted {before} SSTables into 1 ({after} keys)");
    Ok(())
}

fn run_stats(dir: &str) -> std::io::Result<()> {
    let lsm = Lsm::open(dir)?;
    println!("memtable entries: {}", lsm.memtable_len());
    println!("SSTables:         {}", lsm.sstable_count());
    Ok(())
}

/// Experiment: does compaction actually pay for itself, in read cost?
/// Writes enough keys to produce several SSTables (small memtable
/// limit, on purpose), overwrites the SAME keys repeatedly (so most
/// SSTables are full of now-stale duplicates), then times get() before
/// and after compact().
fn run_experiment() -> std::io::Result<()> {
    let dir = std::env::temp_dir().join(format!("tiny-lsm-experiment-{}", process::id()));
    let _ = std::fs::remove_dir_all(&dir);

    let num_keys = 20;
    let overwrites_per_key = 15; // each overwrite is a fresh WAL+memtable entry

    let mut lsm = Lsm::open(&dir)?;
    lsm.memtable_limit = 4; // small on purpose: forces many small SSTables
    for round in 0..overwrites_per_key {
        for k in 0..num_keys {
            lsm.put(&format!("key{k}"), &format!("value-round{round}"))?;
        }
    }
    lsm.flush()?; // flush whatever's left in the memtable

    let sstables_before = lsm.sstable_count();
    // A MISSING key is the worst case for a scan-every-table read
    // path: it can't short-circuit early, so it has to check every
    // SSTable (and, before compaction, most of them hold nothing but
    // now-stale duplicates of the same 20 keys).
    let missing_key_lookups = 50;
    let before_ms = time_missing_key_gets(&lsm, missing_key_lookups)?;

    let (tables_before, keys_after) = lsm.compact()?;
    let after_ms = time_missing_key_gets(&lsm, missing_key_lookups)?;

    println!("Experiment: does compaction pay for itself in read cost?\n");
    println!("{num_keys} keys, each overwritten {overwrites_per_key} times, memtable_limit=4");
    println!("(measuring a MISSING key's lookup cost -- the worst case: every SSTable must be checked)\n");
    println!(
        "before compact: {sstables_before} SSTables, {tables_before} total entries on disk, \
         {missing_key_lookups}x missing-key get() took {before_ms:.3} ms"
    );
    println!(
        "after  compact: 1 SSTable ({keys_after} live keys), \
         {missing_key_lookups}x missing-key get() took {after_ms:.3} ms"
    );
    println!(
        "\nmissing-key lookups were {:.1}x {} after compaction",
        if after_ms > 0.0 { before_ms / after_ms } else { 0.0 },
        if before_ms >= after_ms { "faster" } else { "slower" }
    );

    let _ = std::fs::remove_dir_all(&dir);
    Ok(())
}

fn time_missing_key_gets(lsm: &Lsm, iterations: usize) -> std::io::Result<f64> {
    let start = Instant::now();
    for _ in 0..iterations {
        lsm.get("this-key-was-never-written")?;
    }
    Ok(start.elapsed().as_secs_f64() * 1000.0)
}
