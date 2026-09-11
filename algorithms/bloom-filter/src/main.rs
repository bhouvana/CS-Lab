// CLI:
//   bloom demo
//   bloom experiment
use bloom_filter::{measure_fp_rate, BloomFilter};

fn main() {
    let mode = std::env::args().nth(1).unwrap_or_else(|| "experiment".to_string());
    match mode.as_str() {
        "demo" => demo(),
        "experiment" => experiment(),
        _ => {
            eprintln!("usage: bloom [demo|experiment]");
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
        let verdict = if bf.contains(w.as_bytes()) { "probably present" } else { "definitely absent" };
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
