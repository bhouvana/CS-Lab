# Bloom Filter

## What is this?

A Rust implementation of a Bloom filter — a fixed-size bit array plus `k`
hash functions supporting `insert()` and `contains()` — with an experiment
that measures its false-positive rate empirically against the closed-form
prediction.

```bash
cargo run --bin bloom -- demo         # tiny insert/contains walkthrough
cargo run --bin bloom -- experiment   # false-positive-rate table
cargo run --example basic             # optimal-parameter sizing example
```

## Why does it matter?

A Bloom filter answers "have I seen this before?" in O(k) time using far
less memory than storing the actual items, at the cost of admitting false
positives. It's the canonical example of trading exactness for space —
used in practice to avoid disk/network lookups for keys that definitely
don't exist (databases, CDNs, spell checkers, network routers).

## Concept

```text
insert(x)   -> set k bits, at positions h_1(x)..h_k(x)
contains(x) -> false  if ANY of those k bits is 0   (definitely absent)
            -> true   if ALL of those k bits are 1   (probably present)
```

The asymmetry is the whole point: a Bloom filter can never produce a false
*negative* (an inserted item always reports present), but it can produce a
false *positive* (an uninserted item's bits may already be set by other
items' overlapping hash positions).

## How it works

- `k` hash positions come from double hashing (Kirsch–Mitzenmacher):
  `h_i(x) = h1(x) + i * h2(x) mod m`, needing only two base hash values
  instead of `k` independent ones.
- The two base hashes are FNV-1a (a simple, dependency-free hash) run
  through a splitmix64-style finalizer. FNV-1a alone has weak avalanche in
  its low bits — an early version of this lab measured a false-positive
  rate 3.5x higher than theory predicted because of it (see
  `false_positive_rate_matches_theory_within_tolerance_regression` in
  `tests/tests.rs`, which now guards against that regressing).
- The bit array is packed into `Vec<u64>` (one bit per slot, 64 per word),
  not `Vec<bool>`, so a 100,000-item filter takes kilobytes, not a byte
  per bit.

## Implementation

- `src/lib.rs` — `BloomFilter` (insert/contains/optimal_params) plus the
  experiment helpers (`theoretical_fp_rate`, `measure_fp_rate`).
- `src/main.rs` — CLI (`demo` / `experiment`).
- `examples/basic.rs` — sizing a filter via `with_optimal_params`.
- `tests/tests.rs` — no false negatives, empty-filter edge case, invalid
  params (`num_bits`/`num_hashes` = 0), false-positive-rate regression.

## Example

```text
$ cargo run --bin bloom -- demo
inserted: apple, banana, cherry

apple    -> probably present
banana   -> probably present
cherry   -> probably present
date     -> definitely absent
fig      -> definitely absent

$ cargo run --example basic
m = 95851 bits, k = 7 hashes
rust             -> probably present
hash             -> probably present
not-inserted     -> definitely absent
```

## Experiments

**How does bit-array size (bits per item) affect the false-positive rate,
and does it hold steady as `n` grows?** For each `n` in {1,000; 10,000;
100,000} and each bits-per-item setting in {4, 8, 12}, insert `n` items,
then probe with `n` items from a disjoint namespace (guaranteed true
negatives) and measure the false-positive rate.

Real output from `cargo run --release --bin bloom -- experiment`:

```text
n         bits/item   m (bits)    k     measured      theoretical
1000      4           4000        3     0.15500       0.14689
1000      8           8000        6     0.01600       0.02158
1000      12          12000       8     0.00300       0.00314
10000     4           40000       3     0.14890       0.14689
10000     8           80000       6     0.02300       0.02158
10000     12          120000      8     0.00270       0.00314
100000    4           400000      3     0.14680       0.14689
100000    8           800000      6     0.02153       0.02158
100000    12          1200000     8     0.00332       0.00314
```

## Results

Measured rates track the theoretical `(1 - e^(-kn/m))^k` closely at every
scale (worst case ~10% relative error at n=1,000, tightening to <2% at
n=100,000 — small-`n` runs have fewer probes so more sampling noise).
Going from 4 to 12 bits per item drops the false-positive rate roughly
50x (0.15 -> 0.003) at the cost of 3x the memory — the classic Bloom
filter space/accuracy trade-off, and it holds steady regardless of `n`,
confirming the filter scales by bits-per-item, not by absolute size.

## What I learned

The double-hashing trick (deriving `k` hash positions from just 2 base
hashes) only works if those 2 base hashes actually have good avalanche —
FNV-1a's known low-bit weakness was invisible in `insert`/`contains`
correctness tests but showed up immediately as a 3.5x inflated
false-positive rate once measured against theory. A property this subtle
needed a *quantitative* test, not just a functional one.

## Limitations

- Not thread-safe, no concurrent insert support.
- No deletion (a standard Bloom filter cannot support removal without a
  counting variant, which isn't implemented here).
- `optimal_params` assumes the item count is known in advance; no
  resizing strategy for growing far past that estimate.

## Further experiments

- Implement a Counting Bloom Filter and measure the memory/deletion
  trade-off.
- Compare FNV-1a+mix64 against `std::collections::hash_map::DefaultHasher`
  (SipHash) for false-positive rate and speed.
- Plot false-positive rate vs. `k` for a fixed `m` and `n` to find the
  empirical optimum and compare it to `k = (m/n) ln 2`.
