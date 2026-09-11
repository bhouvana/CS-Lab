# Garbage Collector

## What is this?

A tiny educational mark-and-sweep garbage collector in C: objects form
a reference graph, a root set defines what's reachable from "outside,"
and `gc_collect()` frees everything unreachable from any root.

```bash
./gc_demo
```

## Why does it matter?

Every managed-memory language (Java, Python, Go, JavaScript, ...) has
something like this underneath `new`/object literals. Mark-and-sweep
is the conceptually simplest real GC algorithm, and building it makes
"reachability, not reference count" click as the actual definition of
"alive" — it's also the classic answer to why reference counting alone
can't collect cycles.

## Concept

```text
root
 +-- object A
 |    +-- object B
 |
 +-- object C

object D                (unrooted, unreferenced)
        |
        v
   gc_collect()
        |
        v
A -> retained
B -> retained  (reachable via A)
C -> retained
D -> collected (unreachable from any root)
```

## How it works

- **Mark phase**: starting from every root, visit every reachable
  object and set its `marked` flag. Already-marked objects are
  skipped, which makes this safe on reference **cycles** — the one
  thing naive reference counting can't handle on its own.
- **Iterative, not recursive**: marking uses an explicit worklist
  (a growable array acting as a stack), not C call-stack recursion.
  See What I Learned — an earlier recursive version would overflow the
  stack on a long reference chain, exactly the shape this lab's own
  benchmark uses.
- **Sweep phase**: walk every object in the heap; anything still
  unmarked is unreachable, so free it; anything marked gets its flag
  reset (for the next collection cycle) and survives.
- **Root removal** (`gc_remove_root`) simulates a variable going out of
  scope — the next collection can then discover that an entire subtree
  only that root was keeping alive has become garbage.

## Implementation

- `include/gc.h` — `Object`, `GC`, `CollectStats`, the public API.
- `src/gc.c` — the whole collector (~100 lines).
- `src/main.c` — builds the project directive's exact example graph.
- `src/bench.c` — the collection-time experiment.
- `tests/test_gc.c` — 7 tests: the directive's example, cascading
  collection after a root drop, an unreachable cycle (must not hang),
  a reachable cycle (must survive), a 200,000-deep chain (the stack-
  overflow regression), an empty heap, and idempotent double-collection.

## Example

```text
$ ./gc_demo
Heap before first collection: 4 objects

A -> retained
B -> retained
C -> retained
D -> collected

collected: 1, retained: 3 (of 4)

--- dropping the root reference to A ---
(B is only reachable through A, so it should become garbage too)

A -> collected
B -> collected
C -> retained

collected: 2, retained: 1 (of 3)
```

## Experiments

**How does collection time scale with heap size and the live/garbage
ratio?** `src/bench.c` builds a heap of `n` objects — a live chain of
some length, rooted, plus an unrooted "garbage" chain filling the
rest — and times `gc_collect()`, auto-repeating small runs since
`clock()` can't resolve a sub-millisecond collection directly.

Real output from `make benchmark`:

```text
objects     % live    reps    ms/run
1000        10        85      0.2353
1000        50        73      0.2740
1000        90        59      0.3390
10000       10        7       3.1429
10000       50        8       2.5000
10000       90        5       4.6000
100000      10        1       28.0000
100000      50        1       27.0000
100000      90        1       34.0000
```

## Results

Collection time scales linearly with heap size (~0.2-0.3ms at 1,000
objects, ~2.5-4.6ms at 10,000, ~27-34ms at 100,000 — roughly 10x time per
10x objects), as expected: mark-and-sweep does O(live objects) work in
the mark phase and O(total objects) work in the sweep phase, both
linear. The live/garbage ratio barely matters at a given heap size —
10% and 90% live take about the same time — because the *total* work
(mark the live ones, sweep every one) is dominated by the heap size
`n`, not by how that `n` splits between live and garbage.

## What I learned

The first version of `mark()` was straightforward recursion — one call
per object, recursing into children. It passed every functional test
immediately... until `src/bench.c`'s live set (a single long reference
chain, by design, to isolate live-vs-garbage cost from graph shape)
hit 90,000 objects and the program crashed with a stack overflow.
Recursion depth had been scaling with chain length the whole time; it
just never showed up until a benchmark happened to build a long enough
chain. Rewriting `mark()` around an explicit worklist (iterative DFS)
fixed it, and `test_long_chain_does_not_overflow_the_stack_regression`
now specifically guards against it recurring.

## Limitations

- **Stop-the-world**: collection is a single synchronous pass; no
  incremental or concurrent collection (this simulator has nothing
  else running anyway).
- **No generational hypothesis exploited** — every collection walks
  the entire heap, unlike real GCs that collect young objects far more
  often than old ones.
- **No compaction**: sweeping frees objects in place; there's no
  moving/copying collector here to also demonstrate reduced
  fragmentation (contrast with `os/allocator`'s coalescing, which
  fights the same fragmentation problem from the manual-allocation side).
- `-fsanitize=address,undefined` is a separate `make test-asan` target,
  not the default `make test` — see `os/allocator`'s README for why
  (this repo's MinGW/Windows toolchain has no ASan/UBSan runtime).

## Additional experiments

`src/further_experiments.c` implements the three follow-up experiments as
one deterministic, assertion-backed executable. Run it with
`make experiments`.

### Generational collection

The accounting model compares a full collection of 9,000 old objects and
1,000 young objects with a young-only collection. It includes one remembered
old-to-young reference, so the young collector can find its live young set
without rescanning every old object.

### Reference counting and cycles

The reference-counting model builds `A <-> B`, releases both outside roots,
and shows that neither object reaches zero references. Breaking the cycle then
allows both objects to be released, demonstrating the exact case that
mark-and-sweep handles and naive reference counting does not.

### Compacting sweep

The arena model fills 64 slots, keeps every other object live, relocates live
objects to the front, and updates their slot references through a relocation
table. It measures the same largest-free-run fragmentation metric used by
`os/allocator`.

Real output from `make experiments`:

```text
Generational collection
  full heap work:  19100 objects
  young-only work: 1101 objects
  work reduction:  94.2%

Reference counting
  A <-> B after roots released: 0 objects freed (cycle leaked)
  after breaking the cycle:     2 objects freed

Compacting sweep
  before: 32 free slots, largest=1, fragmentation=96.9%
  after:  32 free slots, largest=32, fragmentation=0.0%
```
