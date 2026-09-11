# Repository Architecture

CS-Lab is a flat collection of independent labs grouped by CS domain. There
is no shared runtime, no shared library, no orchestration layer between
labs — that is deliberate. Coupling labs together would turn a laboratory
into a platform, which is explicitly out of scope (see
[CS-LAB.md](../CS-LAB.md) §1).

## Domain grouping

```text
algorithms/     classic algorithms & data structures (SAT, graphs, bloom filter)
compilers/      language implementation (tiny language, bytecode VM)
compression/    entropy & dictionary coding (Huffman, LZ77)
crypto/         cryptographic primitives from scratch (SHA-256)
architecture/   CPU/memory microarchitecture simulators (cache, branch, pipeline)
runtimes/       language runtime internals (GC; VM lives in compilers/)
databases/      storage engines (tiny LSM)
networking/     socket-level programming (TCP chat)
distributed/    consensus protocols (Raft simulator)
security/       side-channel and security experiments (constant-time compare)
os/             OS-adjacent mechanics (allocator, shell)
assembly/       x86-64 ABI and syscall mechanics
```

## Per-lab structure

Every lab is self-contained and independently buildable:

```text
<lab>/
├── README.md      what/why/concept/how/example/experiments/results/limitations
├── src/
├── tests/
├── examples/
└── Makefile       (or Cargo.toml for Rust labs)
```

A lab never imports code from another lab. If two labs share a genuine
utility (e.g. a graph type), duplicate the ~20 lines rather than introduce a
shared package — the goal is that each lab can be read start to finish
without jumping elsewhere.

## Root-level orchestration

The root `Makefile` discovers active labs (anything with a `Makefile` or
`Cargo.toml` two levels deep) and drives `build`/`test`/`benchmark`/`clean`
across all of them. It has no knowledge of individual lab internals — a new
lab becomes part of `make build` the moment it gets a Makefile or
Cargo.toml, no root-file edits required.

## Status tracking

Lab status (done/planned) is tracked in the root README's lab table, not in
a separate machine-readable file — this repo intentionally has no build
metadata database.
