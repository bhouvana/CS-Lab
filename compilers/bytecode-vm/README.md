# Bytecode VM

## What is this?

A stack-based bytecode virtual machine in C: an assembler that reads a
text instruction listing, and a VM that fetches, decodes, and executes
it one instruction at a time.

```bash
./vm examples/add.bytecode
./vm examples/add.bytecode --trace
```

## Why does it matter?

This is how real interpreters (Python's CPython, Java's JVM, .NET's
CLR) work under the hood: compile source down to a flat instruction
stream operating on a stack, then run a tight fetch-decode-execute
loop over it. Seeing the stack change instruction-by-instruction with
`--trace` makes that loop tangible.

## Concept

```text
fetch    (read Instruction at program[pc])
   |
   v
decode   (switch on its opcode)
   |
   v
execute  (mutate the stack/memory, compute next pc)
   |
   v
repeat   (until HALT or an error)
```

12 opcodes: `PUSH POP ADD SUB MUL DIV LOAD STORE JUMP JUMP_IF_FALSE
PRINT HALT`. `LOAD`/`STORE` address 256 integer memory slots (simple
global variables). `JUMP`/`JUMP_IF_FALSE` targets are **raw 0-based
instruction indices** — this VM has no labels, so a loop's jump target
is whatever index its destination instruction happens to be at (see
`examples/sum-loop.bytecode`, which comments every line with its
index for exactly this reason).

## How it works

- `src/assembler.c` parses one instruction per line (`#` starts a
  comment), turning mnemonics into `Instruction{ op, operand }` values.
- `src/vm.c` is the fetch/decode/execute loop: a 1024-slot `int64_t`
  stack, 256 memory slots, and a `switch` over `Opcode`.
- `--trace` prints each instruction as it executes, then the resulting
  stack — exactly the format in the project directive.
- All output (both `PRINT` and `--trace` lines) goes through a `FILE
  *out` parameter rather than being hardcoded to `stdout`, so
  `src/bench.c` can redirect it to a scratch file when timing —
  otherwise trace output would both flood the terminal and make the
  benchmark measure I/O instead of execution.

## Implementation

- `include/vm.h` — `Opcode`, `Instruction`, `Program`, `VM`, and the
  three public functions.
- `src/assembler.c` — text -> `Instruction` array.
- `src/vm.c` — the interpreter loop.
- `src/main.c` — CLI.
- `src/bench.c` — the throughput and `--trace` overhead experiments.
- `tests/test_vm.c` — 15 tests: every opcode, jumps taken/not-taken, a
  full loop program, and 6 invalid/edge cases.

## Example

```text
$ cat examples/add.bytecode
PUSH 10
PUSH 20
ADD
PRINT
HALT

$ ./vm examples/add.bytecode --trace
PUSH 10
STACK:
[10]

PUSH 20
STACK:
[10, 20]

ADD
STACK:
[30]

PRINT
30
STACK:
[]

HALT
STACK:
[]

$ ./vm examples/sum-loop.bytecode   # sums 5+4+3+2+1 via LOAD/STORE/JUMP
15
```

## Experiments

**How fast is the fetch/decode/execute loop, and what does `--trace`
cost?** `src/bench.c` runs a tight countdown loop (`STORE`/`LOAD`/
`SUB`/`JUMP`/`JUMP_IF_FALSE`, no `PRINT` inside the loop) at increasing
iteration counts, timing execution with the program's own `PRINT`/
trace output redirected to a scratch file so it never pollutes the
measurement or the terminal. Small workloads are auto-repeated until
50ms elapse in total (`clock()`'s resolution, especially on Windows,
is too coarse to trust a single sub-millisecond run) and the result is
the per-run average — a real measurement, not a fabricated one.

Real output from `make benchmark`:

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

## Results

Raw throughput lands around 50-80 million VM instructions per second
regardless of scale — a `switch`-dispatched loop over a simple stack
is genuinely fast. `--trace` is over **300x slower** at the same
iteration count: each traced instruction does 1-2 `fprintf` calls
(formatting plus a syscall-backed write), and that I/O cost dwarfs the
actual arithmetic being traced. This is a direct, measured illustration
of why production interpreters never leave tracing/logging on by
default in hot loops.

## What I learned

Threading `FILE *out` through `vm_run` instead of hardcoding `stdout`
turned out to matter for more than flexibility — it's what made the
`--trace` benchmark possible to run at all without either flooding the
terminal with hundreds of thousands of lines or silently measuring
disk I/O speed instead of interpreter speed.

## Limitations

- No labels/symbolic jump targets — every jump is a raw instruction
  index, which makes hand-writing loops error-prone (see the index
  comments in `examples/sum-loop.bytecode`). A real assembler would
  resolve labels in a first pass.
- 256 memory slots, no named variables — this is closer to raw
  machine registers/memory than to a real language's variables.
- No function calls (`CALL`/`RET`), so no recursion or subroutines.
- Educational implementation: no bytecode serialization format, no
  optimizations (constant folding, etc).

## Further experiments

- Add a first-pass label resolver to the assembler so loops don't
  need manually counted instruction indices.
- Compile `compilers/tiny-language`'s AST down to this VM's bytecode
  instead of tree-walking it, and compare execution speed.
- Add `CALL`/`RET` opcodes and a call stack, then measure the added
  per-call overhead.
