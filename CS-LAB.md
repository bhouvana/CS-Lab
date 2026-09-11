# CS-LAB

# THE COMPUTER SCIENCE LABORATORY

## Mega Project Directive

### Fundamental Computer Science Implementations

### Algorithms · Compilers · Languages · Compression · Cryptography · Architecture · Runtimes · Distributed Systems · Databases · Networking · Security

---

# 0. MISSION

You are building **CS-Lab**.

CS-Lab is not a collection of toy programs.

It is not a tutorial repository.

It is not a web application.

It is not an AI wrapper.

It is not intended to become a multi-week engineering project.

The mission is:

> Build a compact, high-quality laboratory containing small implementations of fundamental Computer Science ideas that can each be understood, executed, tested, benchmarked, and experimented with in roughly one focused day.

The project should make fundamental CS concepts tangible.

A person should be able to clone the repository and explore:

```text
Algorithms
Compilers
Programming Languages
Compression
Cryptography
Computer Architecture
Operating-System Concepts
Runtime Systems
Databases
Networking
Distributed Systems
Security
```

through actual implementations.

The emphasis is:

```text
UNDERSTAND
    ↓
IMPLEMENT
    ↓
EXPERIMENT
    ↓
MEASURE
    ↓
EXPLAIN
```

Every laboratory should answer:

> "What fundamental CS idea does this implementation make concrete?"

---

# 1. HARD SCOPE CONSTRAINT

THIS IS A 1–2 DAY PROJECT.

Do not allow the project to expand into a giant platform.

Do not build:

* authentication
* user accounts
* databases for the website
* cloud deployment
* dashboards
* SaaS infrastructure
* elaborate frontend
* AI features
* unnecessary abstractions
* complicated package management
* enterprise architecture
* microservices
* Docker orchestration
* Kubernetes
* unnecessary CI complexity

The repository itself is the product.

The code is the product.

The experiments are the product.

The documentation is the product.

---

# 2. CORE PHILOSOPHY

Each laboratory must be:

```text
small
correct
readable
experimentable
measurable
educational
```

Prefer:

```text
300 understandable lines
```

over:

```text
3,000 abstracted lines.
```

Prefer:

```text
one concept implemented correctly
```

over:

```text
ten concepts implemented superficially.
```

Do not optimize for LOC.

Optimize for:

```text
conceptual density.
```

---

# 3. LANGUAGE PHILOSOPHY

Do NOT default to TypeScript.

The project should intentionally use different languages where they make conceptual sense.

Primary languages:

```text
C
C++
Rust
Python
Assembly
```

Optional:

```text
Verilog/SystemVerilog
```

Language selection should be intentional.

Example:

```text
C
→ memory management
→ compression
→ cryptography
→ runtime internals

C++
→ algorithms
→ architecture simulation
→ SAT solving
→ data structures

Rust
→ compilers
→ virtual machines
→ distributed systems
→ safe systems programming

Python
→ experimentation
→ reference implementations
→ visualization
→ benchmarking

x86-64 Assembly
→ architecture
→ calling conventions
→ stack behavior
→ instruction execution

SystemVerilog
→ hardware-oriented experiments
```

Do not force every language into every project.

---

# 4. REPOSITORY STRUCTURE

Create:

```text
cs-lab/
│
├── README.md
├── LICENSE
├── Makefile
├── justfile
├── CONTRIBUTING.md
│
├── docs/
│   ├── philosophy.md
│   ├── learning-path.md
│   ├── architecture.md
│   └── experiments.md
│
├── algorithms/
│   ├── sat-solver/
│   ├── graph-algorithms/
│   └── bloom-filter/
│
├── compilers/
│   ├── tiny-language/
│   └── bytecode-vm/
│
├── compression/
│   ├── huffman/
│   └── lz77/
│
├── crypto/
│   └── sha256/
│
├── architecture/
│   ├── cache/
│   ├── branch-predictor/
│   └── pipeline/
│
├── runtimes/
│   ├── garbage-collector/
│   └── stack-machine/
│
├── databases/
│   └── tiny-lsm/
│
├── networking/
│   └── tcp-chat/
│
├── distributed/
│   └── raft-simulator/
│
├── security/
│   └── constant-time/
│
├── os/
│   ├── allocator/
│   └── shell/
│
├── assembly/
│   ├── calling-convention/
│   ├── stack-frames/
│   └── syscall-demo/
│
├── benchmarks/
│
├── scripts/
│
└── tests/
```

IMPORTANT:

If a laboratory cannot realistically be completed within the 1–2 day constraint, reduce its scope.

Do not leave half-built giant systems.

---

# 5. LABORATORY STANDARD

Every laboratory must follow approximately this structure:

```text
<lab>/
│
├── README.md
├── src/
├── tests/
├── examples/
└── Makefile
```

For Rust projects:

```text
Cargo.toml
src/
tests/
README.md
```

For C/C++:

```text
src/
include/
tests/
Makefile
README.md
```

The README must contain:

```text
# What is this?

# Why does it matter?

# Concept

# How it works

# Implementation

# Example

# Experiments

# Results

# What I learned

# Limitations

# Further experiments
```

---

# 6. LAB #1 — SAT SOLVER

Language:

```text
C++
```

Implement a small Boolean SAT solver using:

```text
DPLL
```

Input:

```text
(x1 OR x2)
AND
(NOT x1 OR x3)
AND
(NOT x3 OR x2)
```

Output:

```text
SAT

x1 = false
x2 = true
x3 = true
```

Support:

```text
variables
literals
clauses
CNF
unit propagation
decision variables
backtracking
```

Add simple heuristics:

```text
first-unassigned
most-occurrences
```

Measure:

```text
variables
clauses
decisions
backtracks
runtime
```

Do NOT attempt industrial SAT solving.

The goal is to make NP-completeness and constraint solving tangible.

---

# 7. LAB #2 — GRAPH ALGORITHMS

Language:

```text
C++
```

Implement:

```text
BFS
DFS
Dijkstra
topological sort
```

Use a shared graph representation.

Create small benchmark graphs.

Demonstrate:

```text
BFS
→ shortest path in unweighted graphs

Dijkstra
→ shortest path with non-negative weights

DFS
→ traversal / cycle detection

Topological sort
→ dependency ordering
```

Print actual paths, not merely distances.

---

# 8. LAB #3 — BLOOM FILTER

Language:

```text
Rust
```

Implement:

```text
BloomFilter
```

Support:

```text
insert()
contains()
```

Demonstrate the fundamental property:

```text
false → definitely absent

true → probably present
```

Measure false-positive probability experimentally.

Test:

```text
1,000
10,000
100,000
```

elements.

Compare different:

```text
bit-array sizes
hash counts
```

Generate a small experiment table.

---

# 9. LAB #4 — TINY PROGRAMMING LANGUAGE

Language:

```text
Rust
```

Create:

```text
AtlasLang
```

Example:

```text
let x = 10;
let y = 20;
print(x + y);
```

Implement:

```text
lexer
parser
AST
interpreter
```

Support:

```text
integer literals
variables
arithmetic
comparisons
if
while
print
```

Pipeline:

```text
source
  ↓
lexer
  ↓
tokens
  ↓
parser
  ↓
AST
  ↓
interpreter
```

Keep the language deliberately tiny.

Do NOT implement functions, classes, generics, modules, or a type system.

The purpose is understanding language implementation.

---

# 10. LAB #5 — BYTECODE VM

Language:

```text
C
```

Implement a stack-based bytecode VM.

Example bytecode:

```text
PUSH 10
PUSH 20
ADD
PRINT
HALT
```

VM:

```text
fetch
 ↓
decode
 ↓
execute
 ↓
repeat
```

Implement:

```text
PUSH
POP
ADD
SUB
MUL
DIV
LOAD
STORE
JUMP
JUMP_IF_FALSE
PRINT
HALT
```

Show stack state during execution with:

```text
--trace
```

Example:

```text
PUSH 10

STACK:
[10]

PUSH 20

STACK:
[10, 20]

ADD

STACK:
[30]
```

This laboratory should demonstrate how interpreters and virtual machines work.

---

# 11. LAB #6 — HUFFMAN COMPRESSION

Language:

```text
C
```

Implement:

```text
compress
decompress
```

Pipeline:

```text
input
 ↓
frequency table
 ↓
Huffman tree
 ↓
canonical/prefix codes
 ↓
bit packing
 ↓
compressed file
```

Implement actual binary output.

Do not simply produce text codes.

Support arbitrary bytes.

Measure:

```text
original size
compressed size
compression ratio
```

Test:

```text
English text
repetitive text
random data
source code
```

Explain why random data compresses poorly.

---

# 12. LAB #7 — LZ77

Language:

```text
C++
```

Implement a small sliding-window LZ77 compressor.

Represent matches as:

```text
(offset, length, next_symbol)
```

Demonstrate:

```text
AAAAAAAABAAAAAAA
```

becoming references into the previous window.

Measure:

```text
window size
match length
compressed size
compression ratio
```

Keep the implementation intentionally simple.

Do not implement DEFLATE.

Explain that LZ-style dictionary compression is a foundation for practical compression systems.

---

# 13. LAB #8 — SHA-256 FROM SCRATCH

Language:

```text
C
```

IMPORTANT:

Do NOT use OpenSSL or another crypto implementation for the hashing algorithm.

Implement:

```text
padding
message schedule
compression rounds
rotations
choice
majority
Σ functions
```

Pipeline:

```text
message
 ↓
padding
 ↓
512-bit blocks
 ↓
message schedule
 ↓
64 rounds
 ↓
hash state
 ↓
256-bit digest
```

Verify against known test vectors.

Examples:

```text
""
"abc"
"hello world"
```

Add:

```text
sha256 file
```

command.

IMPORTANT:

Clearly state in the README that this is an educational implementation and should not replace a vetted cryptographic library in production.

---

# 14. LAB #9 — CACHE SIMULATOR

Language:

```text
C++
```

This is one of the most important laboratories.

Implement:

```text
direct-mapped cache
set-associative cache
fully associative cache
```

Parameters:

```text
cache size
line size
associativity
replacement policy
```

Support:

```text
LRU
```

Feed memory traces:

```text
0x1000
0x1004
0x1008
0x2000
0x1000
```

Report:

```text
accesses
hits
misses
hit rate
miss rate
```

Run experiments comparing:

```text
1-way
2-way
4-way
8-way
```

and:

```text
different cache sizes.
```

This should make locality concrete.

---

# 15. LAB #10 — BRANCH PREDICTOR

Language:

```text
C++
```

Implement:

```text
always-taken
always-not-taken
1-bit
2-bit
gshare
```

Input:

```text
T T N T T T N N ...
```

Output:

```text
Predictor       Accuracy
--------------------------------
Always Taken    62.1%
1-bit           74.4%
2-bit           81.2%
GShare          87.3%
```

Use synthetic branch patterns:

```text
always taken
alternating
loop
random
biased
```

Explain why branch prediction exists.

Connect prediction accuracy to processor performance.

Do not build a full CPU here.

Aegis-X86 already serves that purpose.

---

# 16. LAB #11 — PIPELINE SIMULATOR

Language:

```text
Python
```

Simulate:

```text
IF
ID
EX
MEM
WB
```

Input:

```text
ADD
SUB
LOAD
MUL
```

Output a cycle table:

```text
Cycle  1 2 3 4 5 6
ADD    F D E M W
SUB      F D E M W
LOAD       F D E M W
```

Then demonstrate hazards:

```text
RAW
WAR
WAW
```

Implement:

```text
stall
forwarding
```

Compare:

```text
pipeline without forwarding
pipeline with forwarding
```

Measure:

```text
cycles
CPI
stalls
```

This should remain a simulator, not RTL.

---

# 17. LAB #12 — MEMORY ALLOCATOR

Language:

```text
C
```

Implement:

```text
my_malloc
my_free
my_calloc
my_realloc
```

Start with:

```text
free list
first fit
```

Then add:

```text
block splitting
block coalescing
alignment
```

Expose statistics:

```text
allocated bytes
free bytes
number of blocks
fragmentation
```

Create a workload that demonstrates fragmentation.

Do not attempt to replace glibc malloc.

The purpose is understanding heap allocation.

---

# 18. LAB #13 — GARBAGE COLLECTOR

Language:

```text
C
```

Implement a tiny educational:

```text
mark-and-sweep garbage collector
```

Represent objects as nodes.

Construct:

```text
root
 ├── object A
 │    └── object B
 │
 └── object C

object D
```

Run collection.

Expected:

```text
A → retained
B → retained
C → retained
D → collected
```

Demonstrate:

```text
reachability
mark phase
sweep phase
```

Do not attempt a production allocator or concurrent GC.

---

# 19. LAB #14 — TINY LSM DATABASE

Language:

```text
Rust
```

Build a miniature log-structured storage engine.

Support:

```text
put(key, value)
get(key)
```

Architecture:

```text
write
 ↓
WAL
 ↓
memtable
 ↓
flush
 ↓
sorted run
```

Implement a tiny:

```text
SSTable
```

format.

Then implement a simple merge/compaction.

Do NOT build SQL.

Do NOT build transactions.

Do NOT build a query engine.

The purpose is understanding why systems such as modern storage engines use:

```text
append
sort
merge
```

rather than constantly modifying data in place.

---

# 20. LAB #15 — TCP CHAT

Language:

```text
C
```

Build:

```text
server
client
```

using POSIX sockets.

Support:

```text
multiple clients
broadcast messages
disconnect
```

Use:

```text
select()
```

or:

```text
poll()
```

Demonstrate:

```text
socket()
bind()
listen()
accept()
connect()
send()
recv()
```

The purpose is understanding networking at the socket level.

Do not build authentication or a web frontend.

---

# 21. LAB #16 — RAFT SIMULATOR

Language:

```text
Rust
```

IMPORTANT:

This is a simulator, not a production distributed database.

Simulate:

```text
Node A
Node B
Node C
Node D
Node E
```

Implement:

```text
leader election
term
candidate
leader
follower
heartbeat
basic log replication
```

Allow simulated failure:

```text
kill node B
```

Then demonstrate leader election.

Example:

```text
Term 1:
A becomes leader

Term 2:
A fails

Term 3:
C becomes leader
```

Do not implement network transport.

Use deterministic simulated messages.

The objective is understanding distributed consensus.

---

# 22. LAB #17 — CONSTANT-TIME EXPERIMENT

Language:

```text
C
```

Demonstrate why secret-dependent branching can be dangerous.

Create two versions:

```text
insecure_compare()
constant_time_compare()
```

Benchmark them with differing inputs.

Explain:

```text
early exit
timing variation
secret-dependent execution
```

Do NOT claim that a simple benchmark proves cryptographic security.

This is an educational side-channel experiment.

---

# 23. LAB #18 — X86-64 CALLING CONVENTION LAB

Language:

```text
C + Assembly
```

Create:

```text
main.c
math.S
```

Demonstrate:

```text
function arguments
return values
caller-saved registers
callee-saved registers
stack alignment
```

Example:

```c
long add(long a, long b);
```

Assembly:

```asm
add:
    mov %rdi, %rax
    add %rsi, %rax
    ret
```

Show:

```text
C source
 ↓
compiler
 ↓
assembly
 ↓
registers
 ↓
return value
```

Add examples with:

```text
3 arguments
local variables
stack frames
```

---

# 24. LAB #19 — STACK FRAME VISUALIZER

Language:

```text
C + Assembly + Python
```

Create a small C program with nested calls:

```text
main()
 └── foo()
      └── bar()
```

Compile with frame pointers enabled.

Generate a simple textual representation:

```text
STACK

bar()
 ├── local variables
 ├── saved RBP
 └── return address

foo()
 ├── local variables
 ├── saved RBP
 └── return address

main()
```

The purpose is understanding:

```text
RSP
RBP
return addresses
stack frames
function calls
```

---

# 25. LAB #20 — SYSCALL LAB

Language:

```text
C + x86-64 Assembly
```

Demonstrate direct Linux system calls.

Implement a tiny:

```text
write()
exit()
```

without libc for the actual syscall path.

Show:

```text
user program
 ↓
register arguments
 ↓
syscall
 ↓
kernel
 ↓
return value
```

Explain the Linux x86-64 syscall calling convention.

Keep this tiny.

---

# 26. ASSEMBLY POLICY

Assembly laboratories should not become arbitrary Assembly exercises.

Every Assembly experiment must answer a conceptual question.

Examples:

```text
How does a function call work?

How are arguments passed?

Where does the return value go?

What happens to RSP?

What happens during syscall?

What does the compiler generate?
```

Always include:

```bash
objdump
```

or:

```bash
gcc -S
```

examples where useful.

---

# 27. CROSS-LAB CONNECTIONS

The repository should intentionally connect laboratories.

Example:

```text
Tiny Language
      ↓
Bytecode
      ↓
Stack VM
```

Another:

```text
C program
      ↓
Assembly
      ↓
Calling convention
      ↓
Stack frame
      ↓
CPU pipeline
      ↓
Cache
      ↓
Branch predictor
```

Another:

```text
input
 ↓
Huffman
 ↓
LZ77
 ↓
compression experiment
```

Another:

```text
allocator
 ↓
runtime
 ↓
garbage collector
```

Another:

```text
LSM storage
 ↓
network service
 ↓
distributed replication
```

The repository should feel like a connected map of Computer Science rather than unrelated code snippets.

---

# 28. BENCHMARKING

Every laboratory where measurement is meaningful must expose measurable results.

Use simple benchmark output.

Example:

```text
Benchmark
────────────────────────────

Input: 10 MB

Implementation: Huffman

Original:       10,485,760 bytes
Compressed:      6,142,220 bytes

Ratio:           58.57%

Encode time:     42 ms
Decode time:     31 ms
```

Do not fabricate numbers.

All reported numbers must come from actual execution.

---

# 29. EXPERIMENTS

Every laboratory should include at least one experiment.

Examples:

### Cache

```text
Does increasing cache size improve hit rate?
```

### Branch predictor

```text
Which predictor handles loops best?
```

### Bloom filter

```text
How does bit-array size affect false positives?
```

### Huffman

```text
Which datasets compress best?
```

### SAT

```text
How does clause count affect runtime?
```

### Allocator

```text
How does allocation pattern affect fragmentation?
```

### Pipeline

```text
How much does forwarding reduce stalls?
```

### Raft

```text
What happens when the leader fails?
```

Experiments are mandatory because the purpose is not merely implementing algorithms.

It is learning their behavior.

---

# 30. TESTING

Each lab must have tests for its core behavior.

Minimum standard:

```text
normal case
edge case
invalid input
regression case
```

Examples:

SHA-256:

```text
empty string
abc
known test vector
large input
```

Allocator:

```text
allocate
free
coalesce
reallocate
alignment
```

SAT:

```text
SAT formula
UNSAT formula
single variable
contradiction
```

Cache:

```text
hit
miss
conflict
eviction
```

Do not build enormous test suites.

Build meaningful tests.

---

# 31. NO FAKE COMPLETENESS

Never claim:

```text
production-ready
industrial-grade
secure
fully optimized
complete implementation
```

unless that claim is genuinely justified.

Use language such as:

```text
educational implementation
research prototype
minimal implementation
toy model
experimental simulator
```

This repository values intellectual honesty.

---

# 32. DOCUMENTATION STANDARD

Each lab README must answer:

```text
1. What problem does this solve?

2. What CS concept does it demonstrate?

3. What is the algorithm?

4. What is the implementation strategy?

5. What are the important tradeoffs?

6. What did the experiments show?

7. What are the limitations?

8. What could be implemented next?
```

Avoid textbook-length documentation.

Aim for:

```text
1–3 pages per laboratory.
```

---

# 33. ROOT README

The root README should immediately communicate:

```text
CS-Lab
```

with subtitle:

```text
A hands-on laboratory for fundamental Computer Science.
```

Then:

```text
Algorithms
Compilers
Compression
Cryptography
Architecture
Runtimes
Operating Systems
Networking
Databases
Distributed Systems
Security
Assembly
```

Show the repository tree.

Then explain:

> CS-Lab is a collection of small implementations designed to turn abstract Computer Science concepts into executable experiments.

Include a table:

```text
Lab                     Language     Concept

SAT Solver              C++          Constraint solving
Tiny Language           Rust         Compilers
Bytecode VM             C            Runtime systems
Huffman                 C            Compression
LZ77                    C++          Compression
SHA-256                 C            Cryptography
Cache Simulator         C++          Computer architecture
Branch Predictor        C++          CPU architecture
Pipeline Simulator      Python       CPU pipelines
Allocator               C            Memory management
Garbage Collector       C            Runtime systems
Tiny LSM                Rust         Storage systems
TCP Chat                C            Networking
Raft Simulator          Rust         Distributed systems
Calling Convention      C/ASM        Machine-level programming
Stack Frames            C/ASM        Runtime/ABI
Syscall Lab             C/ASM        Operating systems
```

---

# 34. LEARNING PATH

Create:

```text
docs/learning-path.md
```

with:

```text
LEVEL 1 — FUNDAMENTALS

Graph Algorithms
Bloom Filter
Huffman

        ↓

LEVEL 2 — HOW PROGRAMS WORK

Tiny Language
Bytecode VM
Calling Convention
Stack Frames

        ↓

LEVEL 3 — HOW COMPUTERS WORK

Cache
Branch Predictor
Pipeline

        ↓

LEVEL 4 — SYSTEMS

Allocator
Garbage Collector
Syscalls
TCP

        ↓

LEVEL 5 — ADVANCED SYSTEMS

LSM
Raft
Constant-Time Experiments
SAT Solver
```

Explain that the order is recommended but not mandatory.

---

# 35. DESIGN PRINCIPLE

Every lab must have a visible:

```text
INPUT
  ↓
ALGORITHM / MACHINE
  ↓
OUTPUT
```

Examples:

```text
CNF
 ↓
DPLL
 ↓
SAT assignment
```

```text
source code
 ↓
lexer
 ↓
parser
 ↓
AST
 ↓
interpreter
```

```text
memory trace
 ↓
cache
 ↓
hit/miss
```

```text
branch trace
 ↓
predictor
 ↓
prediction accuracy
```

```text
messages
 ↓
Raft state machine
 ↓
leader/log state
```

This makes each laboratory understandable.

---

# 36. CLI-FIRST

Every laboratory should primarily work from the terminal.

Examples:

```bash
./sat examples/example.cnf

./huffman compress input.txt output.huff
./huffman decompress output.huff restored.txt

./sha256 file.txt

./cache --size 32KB --line 64 --assoc 4 trace.txt

./branch --predictor gshare trace.txt

./vm examples/program.bytecode

./raft --nodes 5
```

Do not build a web UI.

Do not build a desktop UI.

Terminal output is sufficient.

---

# 37. BUILD SYSTEM

At the root:

```bash
make build
make test
make benchmark
make clean
```

or:

```bash
just build
just test
just benchmark
```

The exact choice is up to the implementation environment.

The important thing is:

```text
one command to build
one command to test
one command to benchmark
```

---

# 38. CROSS-PLATFORM EXPECTATION

Target:

```text
Linux
```

primarily.

For generic algorithmic projects:

```text
Linux
macOS
Windows
```

may work where practical.

Do not spend the project duration solving platform-specific issues.

POSIX-specific laboratories can explicitly state:

```text
Linux/POSIX
```

---

# 39. CODE QUALITY

Use:

```text
clear naming
small functions
comments explaining WHY
assertions
error handling
```

Avoid:

```text
clever one-liners
unnecessary templates
giant functions
unnecessary abstractions
generated boilerplate
```

For C:

```text
-Wall
-Wextra
-Wpedantic
```

For C++:

```text
-Wall
-Wextra
-Wpedantic
```

For Rust:

```text
cargo fmt
cargo clippy
cargo test
```

---

# 40. SAFETY / CORRECTNESS

Memory-unsafe laboratories should be tested with:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

where applicable.

For C/C++:

```text
-fsanitize=address,undefined
```

Do not hide sanitizer failures.

Fix them.

---

# 41. FINAL INTEGRATION

At the end of implementation, execute:

```bash
make build
make test
make benchmark
```

and verify every laboratory.

Create:

```text
docs/experiments.md
```

containing real experiment results.

Do not manufacture results.

---

# 42. FINAL PROJECT TREE

The final repository should approximately resemble:

```text
cs-lab/
│
├── README.md
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
│   ├── sat-solver/
│   ├── graph-algorithms/
│   └── bloom-filter/
│
├── compilers/
│   ├── tiny-language/
│   └── bytecode-vm/
│
├── compression/
│   ├── huffman/
│   └── lz77/
│
├── crypto/
│   └── sha256/
│
├── architecture/
│   ├── cache/
│   ├── branch-predictor/
│   └── pipeline/
│
├── runtimes/
│   ├── garbage-collector/
│   └── stack-machine/
│
├── databases/
│   └── tiny-lsm/
│
├── networking/
│   └── tcp-chat/
│
├── distributed/
│   └── raft-simulator/
│
├── security/
│   └── constant-time/
│
├── os/
│   ├── allocator/
│   └── shell/
│
├── assembly/
│   ├── calling-convention/
│   ├── stack-frames/
│   └── syscall-demo/
│
├── benchmarks/
│
├── scripts/
│
└── tests/
```

---

# 43. IMPLEMENTATION ORDER

Claude Code MUST NOT attempt to build everything simultaneously.

Implement in this order:

```text
PHASE 1
Repository skeleton
Root README
Build system
Documentation framework

        ↓

PHASE 2
Graph algorithms
Bloom filter
Huffman

        ↓

PHASE 3
Tiny language
Bytecode VM

        ↓

PHASE 4
SHA-256
SAT solver

        ↓

PHASE 5
Cache simulator
Branch predictor
Pipeline simulator

        ↓

PHASE 6
Allocator
Garbage collector
Assembly labs
Syscall lab

        ↓

PHASE 7
Tiny LSM
TCP
Raft

        ↓

PHASE 8
Testing
Benchmarks
Documentation
Final integration
```

However:

**If the project reaches the 1–2 day limit, STOP.**

Do not keep adding features merely to satisfy the tree.

A smaller completed repository is better than a giant unfinished repository.

---

# 44. PRIORITY ORDER IF TIME BECOMES LIMITED

If implementation time is running out, prioritize:

```text
1. Tiny Language
2. Bytecode VM
3. Cache Simulator
4. Branch Predictor
5. SHA-256
6. Huffman
7. SAT Solver
8. Allocator
9. Assembly Labs
10. Raft
11. LSM
12. TCP
```

The first ten are more important than repository breadth.

---

# 45. WHAT NOT TO DO

DO NOT:

```text
turn this into a website

turn this into an educational SaaS

add React

add Next.js

add authentication

add a database for metadata

add AI

add cloud deployment

add unnecessary Docker

add Kubernetes

add microservices

write 10,000 lines for a simple algorithm

copy implementations from tutorials

pretend toy implementations are production-ready

fabricate benchmark numbers

write documentation longer than the implementation

implement every possible feature
```

This is a **laboratory**, not a startup.

---

# 46. QUALITY BAR

A laboratory is complete when:

```text
it builds
AND
it runs
AND
it has tests
AND
it demonstrates the concept
AND
it has at least one experiment
AND
its README explains the concept
```

Do not require perfection.

Require:

```text
correctness
clarity
reproducibility
```

---

# 47. FINAL DEMONSTRATION

The final README should contain a section:

# What CS-Lab Lets You See

```text
A Boolean formula
        ↓
     SAT solver

Source code
        ↓
     Compiler
        ↓
      AST
        ↓
   Bytecode / VM

Data
 ↓
Compression

Message
 ↓
SHA-256

Memory addresses
 ↓
Cache
 ↓
Hit / Miss

Branches
 ↓
Predictor
 ↓
Prediction

Instructions
 ↓
Pipeline
 ↓
Stalls / Forwarding

malloc()
 ↓
Heap
 ↓
Free list

Objects
 ↓
GC
 ↓
Reachability

C function
 ↓
Assembly
 ↓
Registers
 ↓
Stack

Application
 ↓
Syscall
 ↓
Kernel

Storage
 ↓
LSM
 ↓
SSTable

Nodes
 ↓
Raft
 ↓
Consensus
```

The repository should make these relationships visible.

---

# 48. RELATIONSHIP TO AEGIS-X86

CS-Lab must NOT duplicate the full Aegis-X86 processor project.

Aegis-X86 is the deep processor/microarchitecture laboratory.

CS-Lab is the broader Computer Science laboratory.

The distinction is:

```text
CS-Lab
│
├── algorithms
├── compilers
├── languages
├── compression
├── cryptography
├── runtimes
├── databases
├── networking
└── distributed systems

              +

Aegis-X86
│
├── ISA
├── decoder
├── microarchitecture
├── OoO execution
├── speculation
├── caches
├── SIMD
├── memory hierarchy
├── verification
├── FPGA
└── silicon research
```

Aegis-X86 asks:

> How do we construct and research a modern processor?

CS-Lab asks:

> What are the fundamental ideas that make Computer Science work?

The two projects should complement one another.

---

# 49. FINAL PRINCIPLE

The repository should communicate one idea:

> **Computer Science becomes much easier to understand when you build the machinery yourself.**

Do not optimize for GitHub stars.

Do not optimize for project size.

Do not optimize for buzzwords.

Optimize for:

```text
fundamental ideas
+
real implementations
+
experiments
+
measurements
+
understanding
```

Build the smallest implementation that makes each idea real.

Then stop.

# END OF DIRECTIVE
