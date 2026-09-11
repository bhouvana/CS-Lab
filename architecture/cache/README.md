# Cache Simulator

## What is this?

A configurable cache simulator in C++ — direct-mapped, N-way
set-associative, and fully-associative are all the same code with
different parameters, using LRU replacement.

```bash
./cache --size 32KB --line 64 --assoc 4 examples/trace.txt
```

## Why does it matter?

This is arguably the most important lab in the repository for making
"why is my code slow" concrete. Cache behavior — not algorithmic
complexity — dominates real-world performance for most programs, and
it's invisible until you can watch a memory trace turn into hits and
misses.

## Concept

```text
memory trace (a sequence of addresses)
      |
      v
    cache               parameters: size, line size, associativity
      |
      v
  hit / miss    ->    accesses, hits, misses, hit rate, miss rate
```

Associativity is a single parameter, not three separate designs:

```text
associativity = 1                    -> direct-mapped
associativity = size/line_size        -> fully associative (1 set)
anything in between                   -> N-way set-associative
```

`num_sets = size / (line_size * associativity)` in every case; only the
number of sets changes.

## How it works

- Each set is a `std::list<uint64_t>` of tags, **MRU at the front**.
  On a hit, the tag is spliced to the front. On a miss, if the set is
  full the back (LRU) entry is popped before the new tag is pushed to
  the front. This is standard "move-to-front list" LRU — O(associativity)
  per access, which is irrelevant at the associativities real caches
  use (2-16).
- Address -> line number is `address / line_size`; line number ->
  (set, tag) is `(line_number % num_sets, line_number / num_sets)`.
  Real hardware slices the address into bits (requiring power-of-two
  set counts); this simulator uses plain division/modulo instead — see
  Limitations.

## Implementation

- `include/cache.hpp` / `src/cache.cpp` — the `Cache` class.
- `include/parse_size.hpp` / `src/parse_size.cpp` — `"32KB"` -> `32768`.
- `src/main.cpp` — CLI, trace file loader (hex or decimal, one address
  per line).
- `src/bench.cpp` — the four experiments below.
- `tests/test_cache.cpp` — 9 tests: hit, miss, conflict miss, LRU
  eviction order, the exact trace from the project directive, and
  invalid-config rejection.

## Example

```text
$ cat examples/trace.txt
0x1000
0x1004
0x1008
0x2000
0x1000

$ ./cache --size 32KB --line 64 --assoc 4 examples/trace.txt
Cache Configuration
-------------------
Size:          32768 bytes
Line size:     64 bytes
Associativity: 4-way
Sets:          128

Results
-------
Accesses:  5
Hits:      3
Misses:    2
Hit rate:  60.00%
Miss rate: 40.00%
```

(0x1000, 0x1004, 0x1008 share one 64-byte line: miss, hit, hit. 0x2000
is a different line: miss. The final 0x1000 hits again.)

## Experiments

**1. Does associativity fix conflict misses?** 8 "hot" lines placed
exactly `cache_size` bytes apart (so they alias to the identical set at
every associativity, by construction — see `src/bench.cpp`), accessed
round-robin for 200 rounds.

**2. Does increasing cache size improve hit rate?** A fixed 64-line
(4096-byte) working set, scanned sequentially and repeatedly, against
increasing cache sizes.

Real output from `make benchmark`:

```text
Experiment 1: associativity vs. aliasing conflict misses
(8 hot lines all aliasing to the same set, 4096-byte cache, 200 rounds)

ways        hit rate
1           0.00%
2           0.00%
4           0.00%
8           99.50%

Experiment 2: cache size vs. hit rate for a fixed working set
(64-line / 4096-byte working set, 4-way, 200 rounds)

size        hit rate
1KB         0.00%
2KB         0.00%
4KB         99.50%
8KB         99.50%
```

**3. Replacement policy on a workload trace.** The benchmark records
row-major and column-major scans of a 64x64 integer matrix, then replays
the 8192 addresses through a 4-way 4KB cache.

```text
policy      hit rate
LRU         46.88%
FIFO        46.88%
random      54.74%
```

**4. Two-level hierarchy.** The same trace runs through a 1KB 4-way L1
and a 16KB 8-way L2. The reported AMAT uses 1 cycle for an L1 hit, 10
cycles for an L2 hit after an L1 miss, and 100 cycles for memory.

```text
L1 hit rate: 46.88%
L2 hit rate (on L1 misses): 94.12%
AMAT: 8.91 cycles
```

## Results

Both experiments show a **sharp cliff, not a gradual curve** — and
that's the honest, correct result for these access patterns, not a
simplification. In experiment 1, with `ways < 8` an LRU set can never
hold all 8 round-robin-accessed lines at once, so every line is always
evicted before its next use: 0% hit rate at 1/2/4-way. At `ways = 8`,
all 8 fit simultaneously and every access after the first round hits:
99.5% (the 0.5% is exactly the first round's 8 unavoidable cold
misses out of 1600 total accesses). Experiment 2 shows the identical
phenomenon from the size side: below 4KB the 64-line working set
doesn't fit and a sequential scan gets zero reuse; at 4KB and above it
fits completely and hit rate jumps to the same 99.5%.
The matrix trace is less binary: LRU and FIFO both reach 46.88%, while
the deterministic random policy reaches 54.74% for this particular
scan. The result is workload-specific, but it demonstrates why policy
comparisons need a trace rather than a single conflict pattern.

The two-level model turns the 46.88% L1 hit rate into an effective
8.91-cycle access by servicing 94.12% of L1 misses in L2 instead of
going to 100-cycle memory. This is AMAT for the chosen costs, not a
claim about any particular CPU.

## What I learned

It's tempting to expect a smooth "more associativity = better hit
rate" curve, but with a strict round-robin/sequential access pattern
there's no partial credit: an LRU set either holds the *entire* active
working set or it holds effectively none of it long enough to reuse,
because every item gets evicted long before its next access unless
capacity covers the whole set. Real workloads have mixed reuse
distances and *do* show smoother curves — the cliff here is a property
of these specific synthetic (but real, measured) access patterns, not
a bug.

## Limitations

- The production `Cache` class implements LRU only; FIFO, random, and
  the two-level calculation exist in `src/bench.cpp` as experiments.
- Address decomposition uses division/modulo, not bit-slicing, so set
  counts need not be a power of two — a simplification real hardware
  doesn't have (real caches require power-of-two sets for cheap
  bit-masking).
- No write policies (write-through vs. write-back, write-allocate) are
  modeled.
