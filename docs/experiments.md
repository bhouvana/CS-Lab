# Experiment Results

Real output from running each lab's experiments. Numbers here are pasted
from actual `make benchmark` / test runs — nothing is fabricated. This
file grows as labs are implemented; a lab not yet listed has no results
yet.

---

## Graph Algorithms (`algorithms/graph-algorithms`)

**Question:** does Dijkstra's heap overhead show up as graphs grow,
relative to BFS?

Random graphs, average out-degree 4, fixed seed, src=0 dst=n-1:

```text
V         E           BFS (ms)      Dijkstra (ms)
100       393         0.011         0.022
1000      3997        0.030         0.165
10000     39996       0.345         1.962
50000     199996      1.044         15.649
```

From 100 to 50,000 vertices (500x), BFS's runtime grew ~95x while
Dijkstra's grew ~710x — matching the O(V+E) vs O((V+E) log V) complexity
gap directly.

## Bloom Filter (`algorithms/bloom-filter`)

**Question:** how does bit-array size (bits per item) affect the
false-positive rate, and does it hold steady as `n` grows?

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

Measured rates track the theoretical `(1 - e^(-kn/m))^k` closely at
every scale. Going from 4 to 12 bits/item drops the false-positive rate
~50x (0.15 -> 0.003) for 3x the memory, and the rate depends on
bits-per-item, not absolute `n`.

Note: an early version used plain FNV-1a for the two base hashes and
measured a false-positive rate 3.5x higher than theory (0.075 vs 0.022)
because of FNV-1a's weak low-bit avalanche. Adding a splitmix64-style
finalizer fixed it — see the lab's README for details.

## Huffman (`compression/huffman`)

**Question:** which kinds of data compress well, and why does random
data not?

```text
dataset        original     compressed      ratio encode(ms) decode(ms)
english            1257            959     76.29%      0.016      0.018
repetitive         6400           1068     16.69%      0.022      0.011
source_code        1450           1124     77.52%      0.018      0.020
random             6400           6665    104.14%      0.215      0.097
```

Repetitive data compresses best (one symbol dominates the frequency
table). Random data comes out **larger** than the input — uniformly
random bytes carry maximum entropy (8 bits/byte), so Huffman's codes
stay close to 8 bits wide and the format's fixed 268-byte header adds
pure overhead. English text and source code land around 76-78%, with
the fixed header cost weighing more heavily on these small (~1-1.5 KB)
sample files than it would on a larger corpus.

(Updated during the CS-Lab Hardened pass: the original numbers here were
all `0.000`, a real bug -- `clock()`'s coarse resolution on native
Windows/MinGW, not an artifact of a fast operation. Fixed by batching
each measurement until 50ms elapse, same pattern already used in
sha256/bytecode-vm/gc's benchmarks; see `docs/regressions.md` #3.)

## Tiny Language / AtlasLang (`compilers/tiny-language`)

**Experiment:** the pipeline-visibility itself — run the same program
through `tokens`, `ast`, and `run` and confirm each stage's output
lines up (e.g. `a > b` parses to one `Binary` AST node, not three
statements). 10/10 tests pass, including a while-loop Fibonacci
sequence checked against the exact expected values `[0,1,1,2,3,5,8,13]`
and four invalid-input cases (division by zero, undefined variable,
assignment without `let`, syntax error).

## Bytecode VM (`compilers/bytecode-vm`)

**Question:** how fast is the fetch/decode/execute loop, and what does
`--trace` cost?

```text
Benchmark: raw VM throughput (countdown loop, no trace)
(each row averaged over enough repeats to exceed 50ms total)

iterations    reps    ms/run      iterations/sec
100000        26      1.9231      52000000
1000000       4       12.7500     78431373
5000000       1       83.0000     60240964

Benchmark: --trace overhead at 100000 iterations
(output redirected to a file, not the terminal)

no trace:   1.9231 ms/run (26 reps)
trace:    668.0000 ms/run (1 reps)  -> 347.4x slower
```

Raw throughput holds around 50-80M instructions/sec regardless of
scale. `--trace` is over 300x slower at equal iteration count — the
`fprintf` calls per traced instruction dominate the actual arithmetic
being traced, a direct measured example of why tracing/logging stays
off by default in interpreter hot loops.

## SHA-256 (`crypto/sha256`)

**Question:** how does hashing throughput scale with input size?

```text
input     reps    ms/hash     MB/s
1 KB      10148   0.0049      207.8
1 MB      12      4.8333      216.9
16 MB     1       84.0000     199.7
```

Throughput holds steady around 200-220 MB/s across three orders of
magnitude — expected, since SHA-256 does the same fixed per-block work
regardless of total message size. All three official test-vector
outputs (empty string, "abc", "hello world") were cross-checked
against the system's `sha256sum` and matched exactly.

## SAT Solver (`algorithms/sat-solver`)

**Question:** how does clause count affect solver difficulty?

```text
ratio     clauses   %SAT      avg decisions avg backtracks  avg ms
2.00      40        100.00    9.65          0.55            0.0087
3.00      60        100.00    11.15         7.85            0.0203
3.50      70        95.00     13.80         14.30           0.0327
4.00      80        85.00     12.00         15.50           0.0367
4.27      85        65.00     22.35         37.85           0.0782
4.50      90        75.00     12.55         16.70           0.0413
5.00      100       30.00     15.80         28.40           0.0534
6.00      120       5.00      12.60         24.40           0.0534
8.00      160       0.00      7.35          14.70           0.0384
```

Random 3-SAT's well-known "phase transition" is directly visible:
satisfiability rate crosses 50% right around the theoretical ~4.267
threshold, and avg-backtracks/avg-runtime both peak sharply at ratio
4.27 (roughly double the neighboring ratios) — formulas near the
satisfiability threshold are measurably hardest for DPLL, exactly
where SAT theory predicts.

## Cache Simulator (`architecture/cache`)

**Questions:** does associativity fix aliasing conflict misses? does
increasing cache size improve hit rate?

```text
Experiment 1: associativity vs. aliasing conflict misses
ways        hit rate
1           0.00%
2           0.00%
4           0.00%
8           99.50%

Experiment 2: cache size vs. hit rate for a fixed working set
size        hit rate
1KB         0.00%
2KB         0.00%
4KB         99.50%
8KB         99.50%
```

Both show a sharp cliff rather than a gradual curve: with 8 hot lines
aliasing to one set, associativity below 8 gives 0% hit rate (every
line evicted before reuse) and associativity 8 gives 99.5%. Same
phenomenon from the size side: below the 4KB working-set size, 0%;
at or above it, 99.5%.

## Branch Predictor (`architecture/branch-predictor`)

**Question:** which predictor handles which branch pattern best?

```text
Alternating: Always Taken 50.0%, 1-bit 0.1%, 2-bit 50.0%, GShare 99.8%
Loop (9T/1N): Always Taken 90.0%, 1-bit 80.0%, 2-bit 90.0%, GShare 90.0%
Biased (90%): Always Taken 90.4%, 1-bit 82.5%, 2-bit 89.6%, GShare 89.5%
```

1-bit is catastrophic on alternating sequences (0.1%, its exact worst
case); GShare learns the alternating pattern almost perfectly (99.8%)
by indexing on history instead of just the last outcome. 2-bit
genuinely beats 1-bit on loop patterns (90.0% vs 80.0%). GShare does
*not* dominate every pattern — it ties or slightly trails 2-bit on the
loop and biased patterns, an honest result from testing a full spread
of patterns rather than only gshare's best case.

## Pipeline Simulator (`architecture/pipeline`)

**Question:** how much does forwarding reduce stalls, and does that
depend on hazard density?

```text
pattern                 stalls (no fwd) stalls (fwd)  CPI (no fwd)  CPI (fwd)   reduction
independent (0%)        0               0             1.08          1.08        0.0%
mixed (1-in-4)          24              0             1.56          1.08        100.0%
mixed (1-in-2)          48              0             2.04          1.08        100.0%
dependent chain (100%)  98              0             3.04          1.08        100.0%
load-use pairs          50              25            2.08          1.58        50.0%
```

Forwarding eliminates 100% of ALU-to-ALU RAW hazard stalls regardless
of dependency density (even a fully-dependent 50-instruction chain
needs zero stalls) because EX/MEM bypassing lines up exactly with the
pipeline's natural one-cycle-apart cadence. The one hazard forwarding
can't fully erase is load-use: stalls drop by half (2 -> 1 per pair)
but never to zero, since a load's data isn't ready until after MEM.

## Allocator (`os/allocator`)

**Question:** how does allocation pattern affect fragmentation?

```text
Checkerboard (978 blocks x 1024 bytes): free=500848 bytes, largest=1024 bytes, fragmentation=99.8%
  -> request for 3072 contiguous bytes (3 blocks' worth): FAILED
LIFO free order (978 blocks x 1024 bytes): free=1048528 bytes, largest=1048528 bytes, fragmentation=0.0%
```

Freeing every other block leaves ~500KB free but scattered into 490
single-block holes — a 3KB contiguous request fails despite ample
total free memory. Freeing in LIFO order coalesces everything back
into one block: 0% fragmentation. Same total freed, opposite outcome.

## Garbage Collector (`runtimes/garbage-collector`)

**Question:** how does collection time scale with heap size and
live/garbage ratio?

```text
objects     % live    reps    ms/run
1000        10        45      0.4444
10000       50        7       3.0000
100000      90        1       32.0000
```

Collection time scales linearly with heap size (~10x time per 10x
objects); live/garbage ratio barely matters at fixed size, since total
work (mark the live set, sweep every object) is dominated by heap size.
An earlier recursive `mark()` crashed with a stack overflow on a
90,000-object live chain — fixed with an iterative worklist, now a
regression test (`test_long_chain_does_not_overflow_the_stack`).

## Calling Convention (`assembly/calling-convention`)

**Question:** what does a real CALL/RET plus register-passing cost vs.
code the compiler inlines away?

```text
via add2 (real CALL/RET):            230.93 ms  (1.15 ns/call)
via add_inline (compiler-inlined):   278.77 ms  (1.39 ns/call)
```

Surprising, reproducible result: the real function call is *faster*.
Both loops are bottlenecked by store-to-load forwarding through a
`volatile` accumulator; `add2`'s register-only arithmetic gets hidden
behind that latency by out-of-order execution, while the inlined
version's instruction scheduling doesn't hide it as well. Verified with
`objdump` that `add_inline` really was inlined (no `call` in that loop)
before accepting the surprising result as real, and confirmed
reproducible with the loop order swapped.

## Stack Frames (`assembly/stack-frames`)

**Question:** how does local variable count affect compiled stack
frame size?

```text
locals    bytes needed   actual frame (sub $N,%rsp)
8         64             0
16        128            8
32        256            136
```

Up to 8 locals (64 bytes), the compiler emits **no** `sub %rsp` at
all — the x86-64 System V ABI's 128-byte "red zone" absorbs it for
leaf functions. Only once locals exceed 128 bytes does a real stack
allocation appear (136 bytes for the 128-byte excess beyond the red
zone, plus 8 for alignment).

## Syscall Lab (`assembly/syscall-demo`)

**Question:** how much does crossing into the kernel actually cost?

```text
my_write(devnull, ...):    130.75 ms  (130.8 ns/call)
noop_call (no syscall):      0.24 ms  (0.2 ns/call)
ratio: syscall is 534x the cost of a plain call
```

A syscall costs ~534x a plain userspace call (131ns vs 0.2ns) even to
`/dev/null` — exactly why libc buffers stdio instead of calling
`write()` per byte. Separately, `strace` caught a real bug while
building this lab: `my_exit()` (a raw syscall) never flushes libc's
stdio buffers, so `printf` output before it silently vanished until an
explicit `fflush(stdout)` was added.

## LZ77 (`compression/lz77`)

**Question:** compression ratio by data type, and does window size
matter past the distance it needs to cover?

```text
dataset                     original  tokens    compressed  ratio
repetitive (A's + B's)      2000      11        44          2.2%
English text                1201      358       1432        119.2%

window        tokens    compressed   (match ~500 bytes back)
16            700       2800
256           80        320
512           55        220
4096          55        220
```

Repetitive data compresses to 2.2%; English text comes out 19%
*larger* (fixed 4-byte tokens cost more than a lone literal when
matches are rare — exactly why DEFLATE follows LZ77 with Huffman
coding). Window size shows a clean threshold at the actual match
distance (~500 bytes): useless below it, no further benefit above it.

## Tiny LSM (`databases/tiny-lsm`)

**Question:** does compaction pay for itself in read cost?

```text
before compact: 75 SSTables, 75 total entries, 50x missing-key get() took 250.230 ms
after  compact: 1 SSTable (20 live keys), 50x missing-key get() took 12.606 ms
missing-key lookups were 19.8x faster after compaction
```

Collapsing 75 small, mostly-stale SSTables into 1 made missing-key
lookups ~20x faster — the concrete payoff for compaction's cost.

## TCP Chat (`networking/tcp-chat`)

**Question:** how does broadcast latency scale with client count?

```text
clients       ms until all others received the message
2             0.083
8             0.115
32            0.173
```

Roughly linear in client count, as expected from a single `for` loop
doing one `send()` per recipient. Building this lab's benchmark also
crashed the server outright the first time multiple clients
disconnected at once — a `SIGPIPE` from `send()`-ing to an
already-closed socket, whose default action kills the process. Fixed
with one line, `signal(SIGPIPE, SIG_IGN)`.

## Raft Simulator (`distributed/raft-simulator`)

**Question:** what happens when the leader fails, repeatedly?

```text
event               ticks since the previous event
leader elected      150
leader elected      160
leader elected      170
```

A 5-node cluster recovers in roughly the same bounded time after each
of 2 tolerable failures — it doesn't get slower or more fragile as
long as a majority survives.

## Constant-Time Compare (`security/constant-time`)

**Question:** does comparison time leak the mismatch position?

```text
match_len       insecure (ms)   constant-time (ms)
0               0.00            151.00
16              66.00           113.00
32              118.00          117.00
```

`insecure_compare`'s time climbs steadily with how many leading bytes
match (0ms -> 118ms over 2,000,000 calls); `constant_time_compare`
stays in a flat, non-trending noise band regardless. A real,
measurable timing side channel — with the explicit caveat that this
benchmark alone proves nothing about real-world exploitability.

---

All 20 labs from the project directive are now implemented, tested,
and measured.
