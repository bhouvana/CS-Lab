# Learning Path

A recommended order through the labs. Not mandatory — jump to whatever
interests you — but each level roughly builds intuition the next level
assumes.

```text
LEVEL 1 — FUNDAMENTALS
  Graph Algorithms   (traversal, shortest paths)
  Bloom Filter       (probabilistic data structures)
  Huffman            (entropy coding)
        |
        v
LEVEL 2 — HOW PROGRAMS WORK
  Tiny Language      (lexer -> parser -> AST -> interpreter)
  Bytecode VM        (fetch/decode/execute)
  Calling Convention (C -> assembly -> registers)
  Stack Frames       (RSP/RBP, return addresses)
        |
        v
LEVEL 3 — HOW COMPUTERS WORK
  Cache Simulator    (locality, hit/miss)
  Branch Predictor   (speculation)
  Pipeline Simulator (hazards, forwarding, stalls)
        |
        v
LEVEL 4 — SYSTEMS
  Allocator          (heap, free lists, fragmentation)
  Garbage Collector  (mark-and-sweep, reachability)
  Syscall Lab        (user space -> kernel)
  TCP Chat           (sockets)
        |
        v
LEVEL 5 — ADVANCED SYSTEMS
  Tiny LSM           (WAL -> memtable -> SSTable -> compaction)
  Raft Simulator     (leader election, consensus)
  Constant-Time Exp. (timing side channels)
  SAT Solver         (DPLL, NP-completeness)
```

## Cross-lab threads

Some labs deliberately connect:

```text
Tiny Language -> Bytecode -> Stack VM

C program -> Assembly -> Calling convention -> Stack frame
          -> CPU pipeline -> Cache -> Branch predictor

input -> Huffman -> LZ77 -> compression experiment

allocator -> runtime -> garbage collector

LSM storage -> network service -> distributed replication
```

Following one thread end-to-end (e.g. write a C function, compile it, read
the assembly, trace its stack frame, then feed similar instruction patterns
into the pipeline simulator) tends to teach more than working through labs
in isolation.
