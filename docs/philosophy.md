# Philosophy

CS-Lab exists on one bet: **Computer Science becomes much easier to
understand when you build the machinery yourself.**

Reading that a hash table has O(1) average lookup is not the same as writing
one and watching a bad hash function turn it into a linked list. Reading that
caches exploit locality is not the same as feeding a cache simulator a memory
trace and watching the hit rate swing.

## Small over big

```text
300 understandable lines  >  3,000 abstracted lines
one concept done right    >  ten concepts done shallow
```

This repository does not optimize for lines of code, breadth, or feature
completeness. It optimizes for *conceptual density* — the ratio of "aha"
moments to code read.

## The pipeline

Every lab follows the same shape:

```text
UNDERSTAND -> IMPLEMENT -> EXPERIMENT -> MEASURE -> EXPLAIN
```

Implementing without experimenting produces code nobody learns from.
Experimenting without measuring produces vibes, not evidence. Measuring
without explaining produces numbers nobody remembers.

## Honesty over polish

Nothing here is production-ready unless explicitly and correctly labeled as
such (most things aren't). SHA-256 implemented for this repo is for learning
the algorithm, not for hashing passwords in a real system. A toy allocator
teaches free-list mechanics, not glibc's actual heap. Calling something a
"toy model" or "educational implementation" is not a disclaimer bolted on at
the end — it's the accurate description of what these are.

## Why the language changes per lab

A language is picked because it makes the concept clearer, not out of
habit. C exposes raw memory for allocators and compression. Rust's ownership
model fits compilers and concurrent-ish simulators (Raft) where correctness
matters more than raw speed. Python is fine for a pipeline simulator where
the value is the printed cycle table, not performance. See
[CS-LAB.md](../CS-LAB.md) §3 for the full reasoning.
