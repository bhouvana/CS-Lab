//! Minimal Bloom filter: a fixed-size bit array plus `k` hash functions.
//!
//! Fundamental property: `contains()` returning `false` means the item is
//! **definitely absent**; returning `true` means **probably present** (it
//! may be a false positive — the item was never inserted, but its bits
//! happened to already be set by other items). A Bloom filter never
//! produces false negatives.
//!
//! No external hashing crate: two independent-enough 64-bit hashes come
//! from FNV-1a run with two different offset bases, then combined via
//! double hashing (Kirsch–Mitzenmacher): `h_i = h1 + i*h2`.

pub struct BloomFilter {
    bits: Vec<u64>, // packed bitset, 64 bits per word
    num_bits: usize,
    num_hashes: usize,
}

impl BloomFilter {
    pub fn new(num_bits: usize, num_hashes: usize) -> Self {
        assert!(num_bits > 0, "num_bits must be > 0");
        assert!(num_hashes > 0, "num_hashes must be > 0");
        let words = num_bits.div_ceil(64);
        BloomFilter {
            bits: vec![0u64; words],
            num_bits,
            num_hashes,
        }
    }

    /// Bit-array size `m` and hash count `k` that minimize false-positive
    /// probability for `expected_items` at `target_fp_rate`. Standard
    /// formulas: m = -n*ln(p) / (ln 2)^2, k = (m/n)*ln 2.
    pub fn optimal_params(expected_items: usize, target_fp_rate: f64) -> (usize, usize) {
        assert!(expected_items > 0);
        assert!(target_fp_rate > 0.0 && target_fp_rate < 1.0);
        let n = expected_items as f64;
        let ln2 = std::f64::consts::LN_2;
        let m = (-(n * target_fp_rate.ln()) / (ln2 * ln2)).ceil() as usize;
        let k = ((m as f64 / n) * ln2).round().max(1.0) as usize;
        (m.max(1), k)
    }

    pub fn with_optimal_params(expected_items: usize, target_fp_rate: f64) -> Self {
        let (m, k) = Self::optimal_params(expected_items, target_fp_rate);
        Self::new(m, k)
    }

    pub fn insert(&mut self, item: &[u8]) {
        for idx in self.bit_indices(item) {
            self.set_bit(idx);
        }
    }

    /// `false` -> definitely absent. `true` -> probably present.
    pub fn contains(&self, item: &[u8]) -> bool {
        self.bit_indices(item).iter().all(|&idx| self.get_bit(idx))
    }

    pub fn num_bits(&self) -> usize {
        self.num_bits
    }

    pub fn num_hashes(&self) -> usize {
        self.num_hashes
    }

    /// Bytes actually used for the packed bit array -- for the memory
    /// comparison against `CountingBloomFilter::memory_bytes`.
    pub fn memory_bytes(&self) -> usize {
        self.bits.len() * 8
    }

    fn bit_indices(&self, item: &[u8]) -> Vec<usize> {
        fnv_hash_indices(item, self.num_hashes, self.num_bits)
    }

    fn set_bit(&mut self, idx: usize) {
        self.bits[idx / 64] |= 1u64 << (idx % 64);
    }

    fn get_bit(&self, idx: usize) -> bool {
        (self.bits[idx / 64] >> (idx % 64)) & 1 == 1
    }
}

/// A Bloom filter variant that supports removal, by storing a small
/// saturating counter (not just a bit) at each of the `k` positions:
/// insert increments, remove decrements, contains checks "all > 0".
///
/// Trade-off: `num_bits` *bytes* of storage instead of `num_bits / 8`
/// -- one `u8` counter per slot instead of one packed bit -- 8x the
/// memory of `BloomFilter` for the same `m`, measured concretely by
/// `memory_bytes()` below and in the lab's memory-tradeoff experiment.
pub struct CountingBloomFilter {
    counts: Vec<u8>,
    num_bits: usize,
    num_hashes: usize,
}

impl CountingBloomFilter {
    pub fn new(num_bits: usize, num_hashes: usize) -> Self {
        assert!(num_bits > 0, "num_bits must be > 0");
        assert!(num_hashes > 0, "num_hashes must be > 0");
        CountingBloomFilter {
            counts: vec![0u8; num_bits],
            num_bits,
            num_hashes,
        }
    }

    pub fn insert(&mut self, item: &[u8]) {
        for idx in fnv_hash_indices(item, self.num_hashes, self.num_bits) {
            self.counts[idx] = self.counts[idx].saturating_add(1);
        }
    }

    /// Removing an item that was never inserted -- or removing it more
    /// times than it was inserted -- is a caller error this can't detect
    /// in general (counters are shared across every item hashed to that
    /// slot). `saturating_sub` means an over-eager remove degrades
    /// correctness quietly (a still-present item can start reporting
    /// absent, the one false-negative case a plain `BloomFilter` can
    /// never produce) rather than panicking or underflowing.
    pub fn remove(&mut self, item: &[u8]) {
        for idx in fnv_hash_indices(item, self.num_hashes, self.num_bits) {
            self.counts[idx] = self.counts[idx].saturating_sub(1);
        }
    }

    pub fn contains(&self, item: &[u8]) -> bool {
        fnv_hash_indices(item, self.num_hashes, self.num_bits)
            .iter()
            .all(|&idx| self.counts[idx] > 0)
    }

    /// Bytes actually used for the counter array (excludes the struct's
    /// own three machine-word fields).
    pub fn memory_bytes(&self) -> usize {
        self.counts.len()
    }
}

/// The `k` hash-index derivation, factored out so both `BloomFilter` and
/// `CountingBloomFilter` share exactly one implementation (they only
/// differ in what they store at each index: a bit vs. a saturating
/// counter).
fn fnv_hash_indices(item: &[u8], num_hashes: usize, num_bits: usize) -> Vec<usize> {
    // FNV-1a alone has weak avalanche in its low bits, which visibly
    // clustered bit indices and inflated the measured false-positive
    // rate above theory. Running each hash through a splitmix64-style
    // finalizer fixes that (see the false_positive_rate_matches_theory
    // test, which caught the original discrepancy).
    let base = fnv1a(item, FNV_OFFSET_1);
    let h1 = mix64(base);
    let h2 = mix64(base ^ FNV_OFFSET_2);
    (0..num_hashes)
        .map(|i| {
            let combined = h1.wrapping_add((i as u64).wrapping_mul(h2));
            (combined % num_bits as u64) as usize
        })
        .collect()
}

/// The same double-hashing scheme as `fnv_hash_indices`, but the two base
/// hashes come from `std::collections::hash_map::DefaultHasher` (SipHash)
/// instead of FNV-1a+mix64 -- used by `compare_hashers` to measure
/// whether a cryptographically-motivated general-purpose hasher actually
/// buys anything here over the from-scratch one.
fn siphash_hash_indices(item: &[u8], num_hashes: usize, num_bits: usize) -> Vec<usize> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};

    let mut s1 = DefaultHasher::new();
    item.hash(&mut s1);
    let h1 = s1.finish();

    // A second independent-enough hash from the same hasher family: hash
    // the item again with a salt byte appended, the same "reseed" trick
    // fnv_hash_indices uses (xor with a second offset) rather than
    // instantiating a second, differently-keyed hasher type.
    let mut s2 = DefaultHasher::new();
    item.hash(&mut s2);
    0xA5u8.hash(&mut s2);
    let h2 = s2.finish();

    (0..num_hashes)
        .map(|i| {
            let combined = h1.wrapping_add((i as u64).wrapping_mul(h2));
            (combined % num_bits as u64) as usize
        })
        .collect()
}

const FNV_OFFSET_1: u64 = 0xcbf29ce484222325; // standard FNV-1a 64-bit offset basis
const FNV_OFFSET_2: u64 = 0x9e3779b97f4a7c15; // golden-ratio constant, used as a second seed
const FNV_PRIME: u64 = 0x100000001b3;

fn fnv1a(data: &[u8], mut hash: u64) -> u64 {
    for &b in data {
        hash ^= b as u64;
        hash = hash.wrapping_mul(FNV_PRIME);
    }
    hash
}

/// splitmix64 finalizer: full 64-bit avalanche from a handful of
/// xor-shift-multiply rounds. Used to clean up FNV-1a's weak low bits
/// before they're reduced mod m.
fn mix64(mut z: u64) -> u64 {
    z = z.wrapping_add(0x9E3779B97F4A7C15);
    z = (z ^ (z >> 30)).wrapping_mul(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)).wrapping_mul(0x94D049BB133111EB);
    z ^ (z >> 31)
}

/// Closed-form false-positive probability for n items, m bits, k hashes:
/// `(1 - e^(-kn/m))^k`.
pub fn theoretical_fp_rate(n: usize, m: usize, k: usize) -> f64 {
    let (n, m, k) = (n as f64, m as f64, k as f64);
    (1.0 - (-k * n / m).exp()).powf(k)
}

/// Build a filter, insert `n` distinct items, then probe with `n` distinct
/// items from a disjoint namespace (guaranteed never inserted) and report
/// the empirical false-positive rate alongside the theoretical prediction.
pub fn measure_fp_rate(n: usize, bits_per_item: usize) -> ExperimentRow {
    let m = n * bits_per_item;
    let k = ((bits_per_item as f64) * std::f64::consts::LN_2).round().max(1.0) as usize;
    let mut bf = BloomFilter::new(m, k);
    for i in 0..n {
        bf.insert(format!("item-{i}").as_bytes());
    }
    let false_positives = (0..n).filter(|i| bf.contains(format!("absent-{i}").as_bytes())).count();
    ExperimentRow {
        n,
        bits_per_item,
        m,
        k,
        measured_fp_rate: false_positives as f64 / n as f64,
        theoretical_fp_rate: theoretical_fp_rate(n, m, k),
    }
}

pub struct ExperimentRow {
    pub n: usize,
    pub bits_per_item: usize,
    pub m: usize,
    pub k: usize,
    pub measured_fp_rate: f64,
    pub theoretical_fp_rate: f64,
}

/// Runs the same insert-n/probe-n false-positive measurement directly
/// against a raw bit array using whichever `hash_fn` is passed in,
/// rather than going through `BloomFilter` (which is hardwired to
/// `fnv_hash_indices`) -- lets `compare_hashers` run the identical
/// workload through two different hash-index derivations and compare
/// both the resulting false-positive rate and the wall-clock time.
fn measure_fp_rate_with_hasher(
    n: usize,
    m: usize,
    k: usize,
    hash_fn: fn(&[u8], usize, usize) -> Vec<usize>,
) -> (f64, std::time::Duration) {
    let words = m.div_ceil(64);
    let mut bits = vec![0u64; words];

    let start = std::time::Instant::now();
    for i in 0..n {
        for idx in hash_fn(format!("item-{i}").as_bytes(), k, m) {
            bits[idx / 64] |= 1u64 << (idx % 64);
        }
    }
    let false_positives = (0..n)
        .filter(|i| {
            hash_fn(format!("absent-{i}").as_bytes(), k, m)
                .iter()
                .all(|&idx| (bits[idx / 64] >> (idx % 64)) & 1 == 1)
        })
        .count();
    let elapsed = start.elapsed();
    (false_positives as f64 / n as f64, elapsed)
}

pub struct HasherComparisonRow {
    pub n: usize,
    pub m: usize,
    pub k: usize,
    pub fnv_fp_rate: f64,
    pub fnv_elapsed: std::time::Duration,
    pub siphash_fp_rate: f64,
    pub siphash_elapsed: std::time::Duration,
}

/// Compares `fnv_hash_indices` (this lab's from-scratch FNV-1a+mix64)
/// against `siphash_hash_indices` (`DefaultHasher`/SipHash, Rust's
/// std general-purpose hasher) on identical workloads: same `n`, same
/// `m`/`k` (from `optimal_params`), same inserted/probed items.
pub fn compare_hashers(n: usize, bits_per_item: usize) -> HasherComparisonRow {
    let m = n * bits_per_item;
    let k = ((bits_per_item as f64) * std::f64::consts::LN_2).round().max(1.0) as usize;
    let (fnv_fp_rate, fnv_elapsed) = measure_fp_rate_with_hasher(n, m, k, fnv_hash_indices);
    let (siphash_fp_rate, siphash_elapsed) = measure_fp_rate_with_hasher(n, m, k, siphash_hash_indices);
    HasherComparisonRow {
        n,
        m,
        k,
        fnv_fp_rate,
        fnv_elapsed,
        siphash_fp_rate,
        siphash_elapsed,
    }
}

/// Fixes `m` and `n`, sweeps `k`, and reports the measured false-positive
/// rate at each -- finding the empirical optimum and comparing it to the
/// closed-form `k = (m/n) ln 2`.
pub fn sweep_k(n: usize, m: usize, k_values: &[usize]) -> Vec<(usize, f64, f64)> {
    k_values
        .iter()
        .map(|&k| {
            let (measured, _) = measure_fp_rate_with_hasher(n, m, k, fnv_hash_indices);
            (k, measured, theoretical_fp_rate(n, m, k))
        })
        .collect()
}
