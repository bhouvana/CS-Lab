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
        let words = (num_bits + 63) / 64;
        BloomFilter { bits: vec![0u64; words], num_bits, num_hashes }
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

    fn bit_indices(&self, item: &[u8]) -> Vec<usize> {
        // FNV-1a alone has weak avalanche in its low bits, which visibly
        // clustered bit indices and inflated the measured false-positive
        // rate above theory. Running each hash through a splitmix64-style
        // finalizer fixes that (see the false_positive_rate_matches_theory
        // test, which caught the original discrepancy).
        let base = fnv1a(item, FNV_OFFSET_1);
        let h1 = mix64(base);
        let h2 = mix64(base ^ FNV_OFFSET_2);
        (0..self.num_hashes)
            .map(|i| {
                let combined = h1.wrapping_add((i as u64).wrapping_mul(h2));
                (combined % self.num_bits as u64) as usize
            })
            .collect()
    }

    fn set_bit(&mut self, idx: usize) {
        self.bits[idx / 64] |= 1u64 << (idx % 64);
    }

    fn get_bit(&self, idx: usize) -> bool {
        (self.bits[idx / 64] >> (idx % 64)) & 1 == 1
    }
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
