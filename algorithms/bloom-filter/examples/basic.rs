// cargo run --example basic
use bloom_filter::BloomFilter;

fn main() {
    // Size the filter for ~10,000 items at a 1% target false-positive rate.
    let mut bf = BloomFilter::with_optimal_params(10_000, 0.01);
    println!("m = {} bits, k = {} hashes", bf.num_bits(), bf.num_hashes());

    for word in ["rust", "bloom", "filter", "hash"] {
        bf.insert(word.as_bytes());
    }

    for word in ["rust", "hash", "not-inserted"] {
        println!("{word:16} -> {}", if bf.contains(word.as_bytes()) { "probably present" } else { "definitely absent" });
    }
}
