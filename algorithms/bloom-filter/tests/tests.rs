use bloom_filter::{measure_fp_rate, theoretical_fp_rate, BloomFilter};

#[test]
fn no_false_negatives_normal_case() {
    let mut bf = BloomFilter::new(1024, 4);
    let items = ["alpha", "beta", "gamma", "delta", "epsilon"];
    for item in items {
        bf.insert(item.as_bytes());
    }
    for item in items {
        assert!(bf.contains(item.as_bytes()), "{item} was inserted but reported absent");
    }
}

#[test]
fn empty_filter_reports_everything_absent_edge_case() {
    let bf = BloomFilter::new(256, 3);
    assert!(!bf.contains(b"anything"));
    assert!(!bf.contains(b""));
}

#[test]
#[should_panic(expected = "num_bits must be > 0")]
fn zero_bits_is_invalid_input() {
    BloomFilter::new(0, 3);
}

#[test]
#[should_panic(expected = "num_hashes must be > 0")]
fn zero_hashes_is_invalid_input() {
    BloomFilter::new(1024, 0);
}

#[test]
fn false_positive_rate_matches_theory_within_tolerance_regression() {
    // Regression guard: if double-hashing or bit indexing breaks, this
    // measured rate would drift far from the closed-form prediction.
    let row = measure_fp_rate(10_000, 8);
    let theoretical = theoretical_fp_rate(row.n, row.m, row.k);
    assert!(
        (row.measured_fp_rate - theoretical).abs() < 0.03,
        "measured {} vs theoretical {} diverged too far",
        row.measured_fp_rate,
        theoretical
    );
}
