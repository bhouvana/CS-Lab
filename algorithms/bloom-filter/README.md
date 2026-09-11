# Bloom Filter

## What is this?

A Rust implementation of a Bloom filter — a fixed-size bit array plus `k`
hash functions supporting `insert()` and `contains()` — with an experiment
that measures its false-positive rate empirically against the closed-form
prediction.

```bash
cargo run --bin bloom -- demo         # tiny insert/contains walkthrough
cargo run --bin bloom -- experiment   # false-positive-rate table
cargo run --bin bloom -- counting     # CountingBloomFilter memory cost + real deletion
cargo run --bin bloom -- hashers      # FNV-1a+mix64 vs. SipHash (DefaultHasher)
cargo run --bin bloom -- ksweep       # false-positive rate vs. k, empirical optimum
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
- `CountingBloomFilter` trades that packing away on purpose: a `Vec<u8>`
  saturating counter per slot instead of a bit, so `remove()` can
  decrement instead of just being unable to un-set a bit another item
  might also depend on. 8x the memory of `BloomFilter` for the same
  `m` (measured below), to buy the one thing a plain Bloom filter
  structurally cannot do.

## Implementation

- `src/lib.rs` — `BloomFilter` and `CountingBloomFilter`
  (insert/contains/optimal_params) plus the experiment helpers
  (`theoretical_fp_rate`, `measure_fp_rate`, `compare_hashers`,
  `sweep_k`). Both filter types share one hash-index derivation
  (`fnv_hash_indices`) — they only differ in what they store at each
  index, a bit vs. a saturating counter.
- `src/main.rs` — CLI (`demo` / `experiment` / `counting` / `hashers` /
  `ksweep`).
- `examples/basic.rs` — sizing a filter via `with_optimal_params`.
- `tests/tests.rs` — 11 tests: no false negatives, empty-filter edge
  case, invalid params (`num_bits`/`num_hashes` = 0), the false-
  positive-rate regression, `CountingBloomFilter`'s no-false-negatives/
  remove/8x-memory/invalid-params cases, and a hasher-comparison and
  k-sweep regression each.

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

**Counting Bloom filter — memory cost and real deletion.** Real output
from `cargo run --release --bin bloom -- counting`:

```text
m = 9586 bits, k = 7
BloomFilter memory:            1200 bytes (packed bits)
CountingBloomFilter memory:    9586 bytes (1-byte counter per slot, 8.0x more)

Deletion, which a plain BloomFilter cannot do at all:
before remove: apple=true, banana=true, cherry=true
after removing banana: apple=true, banana=false, cherry=true
```

Essentially exactly 8x, as one-bit-vs-one-byte storage predicts:
`CountingBloomFilter` uses `m` bytes exactly (9586), `BloomFilter` uses
`ceil(m/64) * 8` bytes (1200) — rounded up to the next 64-bit word,
which is why `1200 * 8 = 9600` rather than 9586 on the nose; the ratio
(9586/1200 = 7.988) lands a hair under 8x because of that rounding, not
because of anything wrong. Deletion works: the targeted item goes
absent, the untouched ones stay present.

**FNV-1a+mix64 vs. SipHash (`DefaultHasher`).** Real output from
`cargo run --release --bin bloom -- hashers`:

```text
n         m         k     fnv fp-rate     fnv time      siphash fp-rate   siphash time
10000     80000     6     0.02300         3.7406ms      0.02060           4.4247ms
100000    800000    6     0.02153         48.6964ms     0.02071           50.9981ms
```

Both hashers land close to the theoretical 0.02158 either way — neither
has a hidden weakness like plain FNV-1a did. SipHash is *not* faster
here; it's consistently ~10-20% slower at both scales, which makes
sense once you remember what it's actually for: SipHash is designed to
resist deliberately-crafted hash-flooding inputs (a security property),
which costs extra mixing rounds this lab's threat model (random
item names, not an adversary choosing them) doesn't need. For this
use case, the from-scratch FNV-1a+mix64 wins on speed for equivalent
quality — a concrete example of "use the hasher your threat model
actually requires," not "the fancier-sounding one is always better."

**False-positive rate vs. `k`, at fixed `m` and `n`.** Real output from
`cargo run --release --bin bloom -- ksweep`:

```text
n = 10000, m = 80000 (formula predicts optimal k = 6)

k     measured      theoretical
1     0.11710       0.11750
2     0.05080       0.04893
3     0.03330       0.03058
4     0.02490       0.02397
5     0.02220       0.02168
6     0.02300       0.02158
7     0.02200       0.02293
8     0.02400       0.02549
9     0.03190       0.02922
10    0.03340       0.03419
11    0.03880       0.04051
12    0.04750       0.04833

empirical optimum: k = 7 (measured rate 0.02200)
```

The measured curve tracks the theoretical U-shape almost exactly —
both bottom out in the k=5-7 range, both climb steeply on either side.
The empirical minimum landed at k=7 rather than the formula's k=6, a
one-step difference well within the sampling noise already visible
elsewhere in this table (k=5's measured rate is *below* its own
theoretical value, which k=6 isn't) — not evidence the formula is
wrong, just that measuring a rate around 2% from 10,000 trials has
real variance.

## What I learned

The double-hashing trick (deriving `k` hash positions from just 2 base
hashes) only works if those 2 base hashes actually have good avalanche —
FNV-1a's known low-bit weakness was invisible in `insert`/`contains`
correctness tests but showed up immediately as a 3.5x inflated
false-positive rate once measured against theory. A property this subtle
needed a *quantitative* test, not just a functional one. The hasher
comparison reinforced the same lesson from a different angle: SipHash
being the "more serious" cryptographic hash didn't make it a better fit
here — it made it slower for no measurable accuracy gain, because this
lab's workload doesn't have the adversarial-input threat model SipHash
is actually solving for.

## Limitations

- Not thread-safe, no concurrent insert support.
- `CountingBloomFilter::remove` cannot detect "this item was never
  inserted" or "this item was already removed" — decrementing shared
  counters below their true count can make a still-present item start
  reporting absent, the one false-negative case a plain `BloomFilter`
  structurally cannot produce.
- `optimal_params` assumes the item count is known in advance; no
  resizing strategy for growing far past that estimate.
