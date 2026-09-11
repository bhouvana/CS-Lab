# Memory Allocator

## What is this?

A `malloc`/`free`/`calloc`/`realloc` implementation in C over a fixed
1MB static arena — first-fit allocation, block splitting, and
immediate-neighbor coalescing.

```bash
./allocator_demo
```

## Why does it matter?

`malloc()` looks like magic until you've built one: it's really just
bookkeeping over a byte array, with two techniques (splitting and
coalescing) doing all the real work of keeping that bookkeeping from
degrading over time. This lab also makes fragmentation — free memory
that exists but can't be used — into something you can measure and
trigger on purpose, not just something you've read causes bugs.

## Concept

```text
my_malloc(n)
      |
      v
   heap (arena)         a fixed byte array, carved into Block-headed chunks
      |
      v
  free list             first-fit search over free blocks only
      |
      v
 pointer to payload  (or NULL if nothing fits)
```

## How it works

- **Layout**: every block (free or allocated) has a `Block` header
  immediately before its payload, and all blocks form a doubly linked
  list *in address order* covering the whole arena — this is what
  makes O(1) coalescing with physical neighbors possible. Free blocks
  additionally form a second, unordered linked list (the free list)
  so `my_malloc` never has to walk over allocated blocks while
  searching.
- **First-fit**: `my_malloc` walks the free list and takes the first
  block big enough, rather than searching for the *best* fit — simpler
  code, and best-fit's fragmentation advantage is marginal in practice
  for the workloads this lab demonstrates.
- **Splitting**: if a found free block is bigger than needed by more
  than one header's worth plus a minimum payload, the leftover becomes
  a new free block instead of being wasted inside the allocation.
- **Coalescing**: on `my_free`, the freed block is merged with an
  immediately-following free neighbor, then (using the possibly-now-
  larger block) with an immediately-preceding free neighbor — this is
  what keeps LIFO-style alloc/free patterns from fragmenting at all
  (see Experiments).
- **Alignment**: every payload starts 8-byte aligned.
- **`my_realloc`** tries to grow in place by absorbing an immediately
  following free block before falling back to allocate-copy-free.

## Implementation

- `include/allocator.h` — public API + `AllocatorStats`.
- `src/allocator.c` — the whole allocator.
- `src/main.c` — small demo (`make build`).
- `src/bench.c` — fragmentation, fit-policy, internal-waste, and growth experiments.
- `tests/test_allocator.c` — 14 tests: allocate, free+reuse, coalesce,
  split, calloc, realloc (grow/NULL/zero), alignment, and 3
  invalid/edge cases (`malloc(0)`, `free(NULL)`, an allocation bigger
  than the whole arena), plus best-fit selection and fragmentation stats.

## Example

```text
$ ./allocator_demo
Fresh heap
  allocated bytes: 0
  free bytes:      1048528
  blocks:          1 (1 free)
  ...

After freeing the middle block (2000 bytes)
  allocated bytes: 4000
  free bytes:      1044384
  blocks:          4 (2 free)
  largest free:    1042384
  fragmentation:   0.19%
```

## Experiments

**How does allocation pattern affect fragmentation?** `src/bench.c`
fills the 1MB arena to exhaustion with 1024-byte blocks two ways: free
every *other* block (checkerboard — the classic fragmentation trigger)
vs. free them in strict LIFO order (so every free immediately
coalesces).

Real output from `make benchmark`:

```text
First-fit checkerboard (970 blocks x 1024 bytes): free=497560 bytes across 486 blocks, largest=1024 bytes, fragmentation=99.8%
  -> request for 3072 contiguous bytes (3 blocks' worth): FAILED (fragmentation, even though total free bytes are enough)

Best-fit checkerboard (970 blocks x 1024 bytes): free=497560 bytes across 486 blocks, largest=1024 bytes, fragmentation=99.8%
  -> request for 3072 contiguous bytes (3 blocks' worth): FAILED (fragmentation, even though total free bytes are enough)

First-fit LIFO free order (970 blocks x 1024 bytes): free=1048520 bytes across 1 blocks, largest=1048520 bytes, fragmentation=0.0%

Internal fragmentation (9 varied requests): requested=2014 bytes, allocated capacity=2040 bytes, wasted=26 bytes (1.3%)

Growable arena (64KB start, doubling realloc): checkerboard growths=4, LIFO growths=4
```

## Results

Both fit policies free exactly half of 970 identical blocks, so both
end up with roughly the same total free bytes (~498KB checkerboard, the full
arena for LIFO since everything gets freed). The *distribution* of
that free memory is what differs completely: checkerboard leaves 486
scattered single-block (1024-byte) holes — 99.8% fragmentation — and a
request for just 3KB contiguous, trivially satisfiable in principle
given ~500KB free, **fails outright**. LIFO order collapses every
freed block into its already-free neighbor as it goes, ending at a
single free block covering the whole arena — 0% fragmentation. This is
fragmentation made concrete: it's not about *how much* is free, it's
about whether coalescing ever gets a chance to happen. On this uniform
checkerboard workload, best-fit has the same result as first-fit because
every candidate hole is the same size.

Internal fragmentation is measured as allocated capacity minus the
original requested sizes. The varied requests waste 26 bytes, or 1.3%
of allocated capacity, from 8-byte alignment. The benchmark's growth
model starts with a 64KB offset arena and doubles it with `realloc` when
live bytes exceed capacity; both patterns reach the same 1MB peak, so
both require four growths. Because this model stores offsets rather than
live pointers, it demonstrates growth frequency without changing the
fixed-arena allocator's pointer-validity contract.

## What I learned

My first version of this experiment allocated only 200 blocks into a
1MB arena, leaving a large untouched tail of free memory — the
"contiguous request" test kept trivially succeeding from that leftover
tail regardless of how fragmented the checkerboard region was,
completely masking the effect I was trying to demonstrate. Filling the
arena to exhaustion first (allocate until `my_malloc` returns NULL,
*then* run the pattern) was necessary to get an honest result instead
of an accidentally-passing one.

## Limitations

- Fixed 1MB arena, no growth (no `sbrk`/`mmap`) — an allocation request
  that doesn't fit simply fails, it never grows the heap.
- Best-fit is available for comparison, but the public default remains
  first-fit.
- The growable-arena experiment is a benchmark model, not a replacement
  for the fixed arena; moving a backing allocation would invalidate live
  pointers in a real allocator.
- Not thread-safe.
- `-fsanitize=address,undefined` (per CS-LAB.md §40) is a separate
  `make test-asan` target, not the default `make test`: this repo's
  MinGW/Windows toolchain has no ASan/UBSan runtime libraries. Use
  `test-asan` on Linux/macOS/clang.
