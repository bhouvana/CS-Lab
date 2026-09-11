# Bytecode VM

## What is this?

A stack-based bytecode virtual machine in C: an assembler that reads a
text instruction listing, and a VM that fetches, decodes, and executes
it one instruction at a time.

```bash
./vm examples/add.bytecode
./vm examples/add.bytecode --trace
./vm examples/call-ret.bytecode
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

19 opcodes: `PUSH POP ADD SUB MUL DIV MOD LT LE GT GE EQ NE LOAD STORE
JUMP JUMP_IF_FALSE CALL RET PRINT HALT`. `LOAD`/`STORE` address 256
integer memory slots (simple global variables). `JUMP`/`JUMP_IF_FALSE`/
`CALL` targets can be either a raw 0-based instruction index or a
symbolic **label** (`loop:` on its own line) that the assembler
resolves in a first pass — see `examples/sum-loop.bytecode` (raw
indices, hand-counted and comment-annotated) vs.
`examples/sum-loop-labeled.bytecode` (the same program with labels)
for the difference. `CALL`/`RET` give subroutines a separate return-
address stack from the data stack.

## How it works

- `src/assembler.c` parses one instruction per line (`#` starts a
  comment), turning mnemonics into `Instruction{ op, operand }` values.
  Two passes: the first walks the file recording where each label
  (`identifier:` alone on a line) points -- the index of the next real
  instruction after it, since a label defines a position, not an
  instruction of its own -- without emitting anything; the second
  parses every real instruction, resolving a `JUMP`/`JUMP_IF_FALSE`/
  `CALL` operand against that table whenever it isn't a plain integer.
  A raw numeric operand still works exactly as before -- labels are
  additive, not a breaking change to existing `.bytecode` files.
- `src/vm.c` is the fetch/decode/execute loop: a 1024-slot `int64_t`
  data stack, a *separate* 256-slot return-address stack for
  `CALL`/`RET` (so a callee's own PUSH/POP traffic can never corrupt a
  pending return address), 256 memory slots, and a `switch` over
  `Opcode`.
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
- `tests/test_vm.c` — 27 tests: every opcode (including the 6
  comparisons and MOD), jumps taken/not-taken, a full loop program,
  label resolution (including an undefined-label and an end-of-program
  edge case), CALL/RET (a subroutine called twice, RET-without-CALL,
  call-stack overflow), and a 20,000-case deterministic fuzz test.

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

$ ./vm examples/call-ret.bytecode   # CALL/RET a "double" subroutine twice
42
200
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
100000        26      1.9778      50561033
1000000       3       18.6503     53618345
5000000       1       75.2000     66489362

Benchmark: --trace overhead at 100000 iterations
(output redirected to a file, not the terminal)

no trace:   1.9778 ms/run (26 reps)
trace:    179.6630 ms/run (1 reps)  -> 90.8x slower

Benchmark: CALL/RET overhead, at 100000 iterations
(identical loop, with vs. without one CALL+RET pair per iteration)

no call:     1.9778 ms/run (26 reps)
with call:   2.1206 ms/run (24 reps)  -> 1.07x slower, ~1.4ns added per CALL+RET pair
```

## Results

Raw throughput lands around 50-65 million VM instructions per second
regardless of scale — a `switch`-dispatched loop over a simple stack
is genuinely fast. `--trace` is tens of times slower at the same
iteration count (90.8x this run; a from-scratch fixed-seed-free
wall-clock benchmark's exact multiplier varies run to run with system
noise, see `docs/reproducibility.md` -- it has been measured as high as
347x in an earlier run of this same benchmark): each traced instruction
does 1-2 `fprintf` calls (formatting plus a syscall-backed write), and
that I/O cost dwarfs the actual arithmetic being traced. This is a
direct, measured illustration of why production interpreters never
leave tracing/logging on by default in hot loops.

**CALL/RET's own overhead is small — about 1.4ns per pair,** a ~7%
slowdown on this loop. That's cheap by design: `CALL` is one bounds
check plus one array write (the return address) plus a `pc` reassignment;
`RET` is a bounds check plus one array read. Neither touches the data
stack at all, and neither does anything as comparatively expensive as
`JUMP_IF_FALSE`'s stack pop or `PRINT`'s `fprintf` call -- the
measurement matches that design directly rather than surprising it.

## What I learned

Threading `FILE *out` through `vm_run` instead of hardcoding `stdout`
turned out to matter for more than flexibility — it's what made the
`--trace` benchmark possible to run at all without either flooding the
terminal with hundreds of thousands of lines or silently measuring
disk I/O speed instead of interpreter speed.

Adding MOD and the 6 comparison opcodes (needed before
`compilers/tiny-language` could compile a single `if`/`while` condition
to this ISA at all -- see that lab's own Experiments section) was a
reminder that "the VM works" and "the VM is a usable compilation
target" are different claims: every hand-written `.bytecode` example
in this repo got by without a single comparison, so the gap was
invisible until a real higher-level language actually tried to target
this ISA.

## Limitations

- 256 memory slots, no named variables — this is closer to raw
  machine registers/memory than to a real language's variables.
- No local variables per call frame — `CALL`/`RET` share the same 256
  global memory slots as everything else; a recursive subroutine using
  those slots for its own state would clobber its own earlier
  invocation's values (no stack frames, only a return-address stack).
- No parameter-passing convention beyond ordinary memory slots: a
  caller `STORE`s arguments before `CALL`, a callee `LOAD`s them after
  — there's no register-passing or automatic stack-based argument area.
- Educational implementation: no bytecode serialization format, no
  optimizations (constant folding, etc).
