// CLI:
//   bloom demo
//   bloom experiment
//   bloom counting
//   bloom hashers
//   bloom ksweep
use bloom_filter::{compare_hashers, measure_fp_rate, sweep_k, BloomFilter, CountingBloomFilter};

fn main() {
    let mode = std::env::args().nth(1).unwrap_or_else(|| "experiment".to_string());
    match mode.as_str() {
        "demo" => demo(),
        "experiment" => experiment(),
        "counting" => counting(),
        "hashers" => hashers(),
        "ksweep" => ksweep(),
        _ => {
            eprintln!("usage: bloom [demo|experiment|counting|hashers|ksweep]");
            std::process::exit(2);
        }
    }
}

fn demo() {
    let mut bf = BloomFilter::new(64, 3);
    for w in ["apple", "banana", "cherry"] {
        bf.insert(w.as_bytes());
    }
    println!("inserted: apple, banana, cherry\n");
    for w in ["apple", "banana", "cherry", "date", "fig"] {
        let verdict = if bf.contains(w.as_bytes()) {
            "probably present"
        } else {
            "definitely absent"
        };
        println!("{w:8} -> {verdict}");
    }
}

fn experiment() {
    println!("Experiment: false-positive rate vs. bits-per-item, at n = 1,000 / 10,000 / 100,000\n");
    println!(
        "{:<10}{:<12}{:<12}{:<6}{:<14}{:<14}",
        "n", "bits/item", "m (bits)", "k", "measured", "theoretical"
    );
    for &n in &[1_000usize, 10_000, 100_000] {
        for &bpi in &[4usize, 8, 12] {
            let row = measure_fp_rate(n, bpi);
            println!(
                "{:<10}{:<12}{:<12}{:<6}{:<14.5}{:<14.5}",
                row.n, row.bits_per_item, row.m, row.k, row.measured_fp_rate, row.theoretical_fp_rate
            );
        }
    }
}

fn counting() {
    println!("Experiment: CountingBloomFilter memory cost vs. BloomFilter, and real deletion\n");

    let (m, k) = BloomFilter::optimal_params(1000, 0.01);
    let bf = BloomFilter::new(m, k);
    let cbf = CountingBloomFilter::new(m, k);
    println!("m = {m} bits, k = {k}");
    println!(
        "BloomFilter memory:         {:>7} bytes (packed bits)",
        bf.memory_bytes()
    );
    println!(
        "CountingBloomFilter memory: {:>7} bytes (1-byte counter per slot, {:.1}x more)",
        cbf.memory_bytes(),
        cbf.memory_bytes() as f64 / bf.memory_bytes() as f64
    );

    println!("\nDeletion, which a plain BloomFilter cannot do at all:");
    let mut cbf = CountingBloomFilter::new(m, k);
    for w in ["apple", "banana", "cherry"] {
        cbf.insert(w.as_bytes());
    }
    println!(
        "before remove: apple={}, banana={}, cherry={}",
        cbf.contains(b"apple"),
        cbf.contains(b"banana"),
        cbf.contains(b"cherry")
    );
    cbf.remove(b"banana");
    println!(
        "after removing banana: apple={}, banana={}, cherry={}",
        cbf.contains(b"apple"),
        cbf.contains(b"banana"),
        cbf.contains(b"cherry")
    );
}

fn hashers() {
    println!("Experiment: FNV-1a+mix64 (this lab's from-scratch hash) vs. DefaultHasher/SipHash (Rust std)\n");
    println!(
        "{:<10}{:<10}{:<6}{:<16}{:<14}{:<18}{:<14}",
        "n", "m", "k", "fnv fp-rate", "fnv time", "siphash fp-rate", "siphash time"
    );
    for &n in &[10_000usize, 100_000] {
        let row = compare_hashers(n, 8);
        println!(
            "{:<10}{:<10}{:<6}{:<16.5}{:<14?}{:<18.5}{:<14?}",
            row.n, row.m, row.k, row.fnv_fp_rate, row.fnv_elapsed, row.siphash_fp_rate, row.siphash_elapsed
        );
    }
}

fn ksweep() {
    println!("Experiment: false-positive rate vs. k, at fixed m and n\n");
    let n = 10_000;
    let m = 80_000; // bits_per_item = 8, same as one row of `experiment`
    let optimal_k = ((m as f64 / n as f64) * std::f64::consts::LN_2).round() as usize;
    println!("n = {n}, m = {m} (formula predicts optimal k = {optimal_k})\n");
    println!("{:<6}{:<14}{:<14}", "k", "measured", "theoretical");
    let k_values: Vec<usize> = (1..=12).collect();
    let mut best_k = k_values[0];
    let mut best_rate = f64::MAX;
    for (k, measured, theoretical) in sweep_k(n, m, &k_values) {
        println!("{k:<6}{measured:<14.5}{theoretical:<14.5}");
        if measured < best_rate {
            best_rate = measured;
            best_k = k;
        }
    }
    println!("\nempirical optimum: k = {best_k} (measured rate {best_rate:.5})");
}
