# CS-Lab

A hands-on laboratory for fundamental Computer Science.

CS-Lab is a collection of small implementations designed to turn abstract Computer Science concepts into executable experiments. Each lab is scoped to roughly one focused day: understand the idea, implement it, run experiments, measure results, explain what happened.

```text
UNDERSTAND -> IMPLEMENT -> EXPERIMENT -> MEASURE -> EXPLAIN
```

Full project directive: [CS-LAB.md](CS-LAB.md).

## Domains

Algorithms · Compilers · Compression · Cryptography · Architecture · Runtimes ·
Operating Systems · Networking · Databases · Distributed Systems · Security · Assembly

## Repository layout

```text
cs-lab/
├── README.md
├── CS-LAB.md            project directive
├── LICENSE
├── Makefile
├── CONTRIBUTING.md
│
├── docs/
│   ├── philosophy.md
│   ├── learning-path.md
│   ├── architecture.md
│   └── experiments.md
│
├── algorithms/
│   ├── sat-solver/          C++    DPLL SAT solving
│   ├── graph-algorithms/    C++    BFS/DFS/Dijkstra/topo-sort
│   └── bloom-filter/        Rust   probabilistic set membership
│
├── compilers/
│   ├── tiny-language/       Rust   lexer/parser/AST/interpreter
│   └── bytecode-vm/         C      stack machine
│
├── compression/
│   ├── huffman/             C      entropy coding
│   └── lz77/                C++    dictionary compression
│
├── crypto/
│   └── sha256/              C      hash function from scratch
│
├── architecture/
│   ├── cache/               C++    cache simulation
│   ├── branch-predictor/    C++    prediction schemes
│   └── pipeline/            Python 5-stage pipeline, hazards
│
├── runtimes/
│   ├── garbage-collector/   C      mark-and-sweep
│   └── stack-machine/       (see compilers/bytecode-vm)
│
├── databases/
│   └── tiny-lsm/            Rust   log-structured storage
│
├── networking/
│   └── tcp-chat/            C      POSIX sockets
│
├── distributed/
│   └── raft-simulator/      Rust   consensus, leader election
│
├── security/
│   └── constant-time/       C      timing side-channel demo
│
├── os/
│   ├── allocator/           C      malloc/free/realloc
│   └── shell/                      (planned)
│
├── assembly/
│   ├── calling-convention/  C+ASM  x86-64 ABI
│   ├── stack-frames/        C+ASM  RSP/RBP visualization
│   └── syscall-demo/        C+ASM  raw Linux syscalls
│
├── benchmarks/
├── scripts/
└── tests/
```

## Lab index

| Lab | Language | Concept | Status |
|---|---|---|---|
| Graph Algorithms | C++ | Traversal, shortest paths, topo sort | done |
| Bloom Filter | Rust | Probabilistic data structures | done |
| Huffman | C | Entropy coding / compression | done |
| SAT Solver | C++ | Constraint solving / NP-completeness | done |
| Tiny Language | Rust | Compilers | done |
| Bytecode VM | C | Runtime systems | done |
| LZ77 | C++ | Dictionary compression | planned |
| SHA-256 | C | Cryptography | done |
| Cache Simulator | C++ | Computer architecture / locality | done |
| Branch Predictor | C++ | CPU architecture | done |
| Pipeline Simulator | Python | CPU pipelines / hazards | done |
| Allocator | C | Memory management | done |
| Garbage Collector | C | Runtime systems | done |
| Tiny LSM | Rust | Storage systems | planned |
| TCP Chat | C | Networking | planned |
| Raft Simulator | Rust | Distributed systems | planned |
| Constant-Time Compare | C | Side channels | planned |
| Calling Convention | C/ASM | Machine-level programming | done (Linux) |
| Stack Frames | C/ASM | Runtime/ABI | done (Linux) |
| Syscall Lab | C/ASM | Operating systems | done (Linux) |

See [docs/learning-path.md](docs/learning-path.md) for a suggested order.

## Building

```bash
make build       # build every implemented lab
make test        # run every lab's tests
make benchmark   # run every lab's experiments/benchmarks
make clean       # remove build artifacts
```

Each lab also builds and tests standalone from its own directory (`make` / `cargo test`).

The three `assembly/*` labs are Linux x86-64 only — their Makefiles
detect the platform and print a `skip:` line instead of building (not
an error) on anything else, because the hand-written assembly assumes
Linux-specific conventions (the System V calling convention, real
Linux syscall numbers) that would produce wrong results or undefined
behavior, not a clean build failure, under a different ABI. They were
built and verified on real Linux (WSL Ubuntu) during development.

## What CS-Lab lets you see

```text
A Boolean formula        -> SAT solver          -> SAT/UNSAT assignment
Source code               -> lexer/parser/AST    -> bytecode/VM execution
Data                      -> Huffman / LZ77       -> compressed bytes
Message                   -> SHA-256              -> 256-bit digest
Memory addresses          -> cache                -> hit/miss
Branch history            -> predictor            -> prediction accuracy
Instructions               -> pipeline             -> stalls/forwarding
malloc()                  -> heap/free list       -> allocation stats
Objects + roots           -> mark-and-sweep GC     -> reachable vs collected
C function call           -> assembly/registers    -> stack frame
Application               -> syscall               -> kernel
Writes                    -> WAL/memtable/SSTable  -> LSM storage
Node failures             -> Raft                  -> new leader
```

## Scope

This is a laboratory, not a startup. No web UI, no auth, no cloud deploy, no
frameworks-for-their-own-sake. Terminal in, terminal out. See
[CS-LAB.md](CS-LAB.md) for the full constraints and [docs/philosophy.md](docs/philosophy.md)
for the reasoning.

Implementations here are educational: toy models and research prototypes, not
production-grade or security-audited software (crypto and security labs say
so explicitly).

## Relationship to Aegis-X86

Aegis-X86 (a separate project) is the deep processor/microarchitecture
laboratory: ISA, OoO execution, speculation, FPGA. CS-Lab is the broader map
of CS fundamentals; the two complement rather than duplicate each other.
