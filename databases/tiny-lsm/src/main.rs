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
    run_bloom_experiment()?;
    run_sparse_index_experiment()?;
    run_automatic_compaction_experiment()?;
    Ok(())
}

fn run_bloom_experiment() -> std::io::Result<()> {
    let dir = experiment_dir("bloom");
    let mut lsm = Lsm::open(&dir)?;
    populate_duplicate_tables(&mut lsm, 20, 15)?;
    let iterations = 50;
    let sparse_ms = time_gets(&lsm, iterations, Lsm::get_sparse)?;
    let filtered_ms = time_gets(&lsm, iterations, Lsm::get)?;
    println!("Experiment 1: Bloom filters for missing-key reads");
    println!("{} SSTables, {iterations} missing-key lookups", lsm.sstable_count());
    println!("sparse index without Bloom filter: {sparse_ms:.3} ms");
    println!("Bloom filter enabled:              {filtered_ms:.3} ms");
    println!(
        "Bloom-filter speedup: {:.1}x\n",
        if filtered_ms > 0.0 {
            sparse_ms / filtered_ms
        } else {
            0.0
        }
    );
    let _ = std::fs::remove_dir_all(&dir);
    Ok(())
}

fn run_sparse_index_experiment() -> std::io::Result<()> {
    let dir = experiment_dir("sparse-index");
    let mut lsm = Lsm::open(&dir)?;
    let entries = 1_000;
    lsm.memtable_limit = entries;
    for i in 0..entries {
        lsm.put(&format!("key-{i:05}"), &format!("value-{i}"))?;
    }
    let iterations = 30;
    let linear_ms = time_gets(&lsm, iterations, Lsm::get_linear)?;
    let sparse_ms = time_gets(&lsm, iterations, Lsm::get_sparse)?;
    println!("Experiment 2: sparse in-file index");
    println!("1 SSTable, {entries} entries, {iterations} missing-key lookups");
    println!("linear scan: {linear_ms:.3} ms");
    println!("sparse index (one offset per 100 entries): {sparse_ms:.3} ms");
    println!(
        "sparse-index speedup: {:.1}x\n",
        if sparse_ms > 0.0 { linear_ms / sparse_ms } else { 0.0 }
    );
    let _ = std::fs::remove_dir_all(&dir);
    Ok(())
}

fn run_automatic_compaction_experiment() -> std::io::Result<()> {
    let dir = experiment_dir("automatic-compaction");
    let mut lsm = Lsm::open(&dir)?;
    lsm.memtable_limit = 20;
    lsm.auto_compact_threshold = Some(8);
    let checkpoints = [1, 8, 9, 16, 24, 40];
    println!("Experiment 3: automatic compaction threshold");
    println!("threshold=8 SSTables, 20 overwritten keys per round");
    println!("round  SSTables  20x missing-key gets (ms)");
    for round in 1..=40 {
        for key in 0..20 {
            lsm.put(&format!("key-{key}"), &format!("round-{round}"))?;
        }
        if checkpoints.contains(&round) {
            let lookup_ms = time_gets(&lsm, 20, Lsm::get)?;
            println!("{round:>5}  {:>8}  {lookup_ms:>27.3}", lsm.sstable_count());
        }
    }
    println!();
    let _ = std::fs::remove_dir_all(&dir);
    Ok(())
}

fn experiment_dir(name: &str) -> std::path::PathBuf {
    let dir = std::env::temp_dir().join(format!("tiny-lsm-{name}-{}", process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    dir
}

fn populate_duplicate_tables(lsm: &mut Lsm, num_keys: usize, rounds: usize) -> std::io::Result<()> {
    lsm.memtable_limit = 4;
    for round in 0..rounds {
        for key in 0..num_keys {
            lsm.put(&format!("key-{key}"), &format!("round-{round}"))?;
        }
    }
    lsm.flush()
}

fn time_gets(
    lsm: &Lsm,
    iterations: usize,
    get: fn(&Lsm, &str) -> std::io::Result<Option<String>>,
) -> std::io::Result<f64> {
    let start = Instant::now();
    for _ in 0..iterations {
        get(lsm, "this-key-was-never-written")?;
    }
    Ok(start.elapsed().as_secs_f64() * 1000.0)
}
