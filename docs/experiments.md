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
english            1257            959     76.29%      0.000      0.000
repetitive         6400           1068     16.69%      0.000      0.000
source_code        1450           1124     77.52%      0.000      0.000
random             6400           6665    104.14%      0.000      0.000
```

Repetitive data compresses best (one symbol dominates the frequency
table). Random data comes out **larger** than the input — uniformly
random bytes carry maximum entropy (8 bits/byte), so Huffman's codes
stay close to 8 bits wide and the format's fixed 268-byte header adds
pure overhead. English text and source code land around 76-78%, with
the fixed header cost weighing more heavily on these small (~1-1.5 KB)
sample files than it would on a larger corpus.

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

---

*(Results for further labs are appended here as they're implemented.)*
