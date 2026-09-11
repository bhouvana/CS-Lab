use bloom_filter::{compare_hashers, measure_fp_rate, sweep_k, theoretical_fp_rate, BloomFilter, CountingBloomFilter};

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
fn counting_filter_no_false_negatives_normal_case() {
    let mut cbf = CountingBloomFilter::new(1024, 4);
    let items = ["alpha", "beta", "gamma"];
    for item in items {
        cbf.insert(item.as_bytes());
    }
    for item in items {
        assert!(cbf.contains(item.as_bytes()));
    }
}

#[test]
fn counting_filter_remove_makes_item_absent_normal_case() {
    let mut cbf = CountingBloomFilter::new(4096, 4); // large m: removing banana shouldn't
                                                     // accidentally clear apple/cherry's bits too
    for w in ["apple", "banana", "cherry"] {
        cbf.insert(w.as_bytes());
    }
    cbf.remove(b"banana");
    assert!(!cbf.contains(b"banana"));
    assert!(cbf.contains(b"apple"));
    assert!(cbf.contains(b"cherry"));
}

#[test]
fn counting_filter_uses_8x_the_memory_of_a_plain_filter_regression() {
    // The whole point of the trade-off this filter exists for: 1 byte
    // per slot instead of 1 bit. If this ever drifts (e.g. someone
    // "optimizes" to a smaller counter width without updating the
    // claim), this regresses loudly instead of silently.
    let bf = BloomFilter::new(8000, 6);
    let cbf = CountingBloomFilter::new(8000, 6);
    assert_eq!(cbf.memory_bytes(), bf.memory_bytes() * 8);
}

#[test]
#[should_panic(expected = "num_bits must be > 0")]
fn counting_filter_zero_bits_is_invalid_input() {
    CountingBloomFilter::new(0, 3);
}

#[test]
fn hasher_comparison_both_hashers_stay_near_theory_regression() {
    // Not "SipHash is better" or "FNV is better" -- just that neither
    // derivation has a hidden avalanche bug like the original FNV-1a-
    // alone version did (see the regression test below).
    let row = compare_hashers(10_000, 8);
    let theoretical = theoretical_fp_rate(row.n, row.m, row.k);
    assert!((row.fnv_fp_rate - theoretical).abs() < 0.03);
    assert!((row.siphash_fp_rate - theoretical).abs() < 0.03);
}

#[test]
fn k_sweep_finds_a_minimum_near_the_formula_normal_case() {
    let n = 10_000;
    let m = 80_000; // bits_per_item = 8 -> formula predicts k = 6
    let k_values: Vec<usize> = (1..=12).collect();
    let results = sweep_k(n, m, &k_values);
    let (best_k, _, _) = results
        .iter()
        .copied()
        .min_by(|a, b| a.1.partial_cmp(&b.1).unwrap())
        .unwrap();
    // Sampling noise means the empirical best k won't necessarily be
    // exactly 6, but it should land in the formula's immediate
    // neighborhood, not somewhere the theory says should be much worse.
    assert!(
        (4..=8).contains(&best_k),
        "empirical best k={best_k} far from formula's k=6"
    );
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
