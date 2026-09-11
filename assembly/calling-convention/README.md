# Calling Convention

**Linux/POSIX, x86-64 only.** Built and verified via WSL Ubuntu
(GCC 15.2.0, real `as`/`ld`/`objdump`) — this is genuinely Linux-only,
not a Windows port of the concept.

## What is this?

Hand-written x86-64 assembly (`src/math.S`) implementing the System V
calling convention's rules directly — register argument passing,
callee-saved register preservation, and stack alignment — called from
C, with a runtime proof (not just a comment) that the callee-saved
contract actually holds.

```bash
make build && ./demo
```

## Why does it matter?

Every C function call follows a contract that's invisible unless you
write assembly by hand: which registers carry arguments, which the
callee must restore before returning, and why the stack has to be
16-byte aligned before a `call`. This lab makes that contract
falsifiable — `check_rbx_preserved()` actually checks it at runtime
rather than asserting it in a comment.

## Concept

```text
C source (extern declarations)
      |
      v
compiler                    generates the CALL, sets up arguments in registers
      |
      v
registers                   RDI, RSI, RDX, RCX, R8, R9 -> args; RAX -> return
      |
      v
hand-written assembly       src/math.S: the callee's side of the contract
      |
      v
return value (RAX)
```

## How it works

- **`add2`/`add3`**: the plain case — arguments arrive in RDI/RSI/RDX,
  result goes in RAX, no stack involved at all.
- **`uses_callee_saved`**: clobbers RBX (a callee-saved register), so
  it `push`es it on entry and `pop`s it before `ret` — that save/
  restore *is* the ABI contract, not decoration.
- **`check_rbx_preserved`**: sets RBX to a sentinel, calls
  `uses_callee_saved`, and checks RBX still holds the sentinel
  afterward. This is what makes the contract testable rather than
  merely documented — `tests/test_calling_convention.c` asserts on its
  return value.
- **`sum_three_locals`**: spills its 3 arguments to local stack slots,
  reloads them, and calls `add3` — demonstrating that RSP stays
  16-byte aligned immediately before that nested `CALL` even with
  locals in play (32 bytes of locals keeps the arithmetic aligned;
  see the comment in `src/math.S`).

## Implementation

- `src/math.S` — every hand-written function, each commented with
  which part of the ABI it demonstrates.
- `src/main.c` — calls each one and prints results.
- `src/bench.c` — the CALL/RET overhead experiment.
- `src/misaligned.c` — opt-in libc probe for the misaligned-stack failure mode.
- `examples/add_equivalent.c` — the C source compared against `add2`
  via `make objdump`.
- `tests/test_calling_convention.c` — 6 tests, including the runtime
  callee-saved and floating-point register proofs.

## Example

```text
$ make objdump
=== add2, hand-written (src/math.S), disassembled from the binary ===
0000000000001209 <add2>:
    1209:	48 89 f8             	mov    %rdi,%rax
    120c:	48 01 f0             	add    %rsi,%rax
    120f:	c3                   	ret

=== add2_c, compiler-generated from examples/add_equivalent.c (-O0 -S) ===
add2_c:
	endbr64
	pushq	%rbp
	movq	%rsp, %rbp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	-8(%rbp), %rdx
	movq	-16(%rbp), %rax
	addq	%rdx, %rax
	popq	%rbp
	ret
```

Same function, same result — but `-O0` sets up a full stack frame and
round-trips both arguments through memory instead of just using the
registers they arrived in. That's not the compiler being bad at its
job; `-O0` deliberately maps each C statement straight to memory
traffic so the assembly stays easy to correlate line-by-line with the
source while debugging. `-O2` on the same source collapses to
something close to `add2`'s 3 instructions.

## Experiments

**What does a real CALL/RET plus register-passing actually cost,
compared to code the compiler inlines away entirely?** `src/bench.c`
runs 200 million iterations of `sink += add2(i, 1)` (a real
cross-file `call`, since GCC can't inline across a `.S` file) against
`sink += add_inline(i, 1)` (a trivial `static` function GCC fully
inlines at `-O2` — confirmed by disassembling the binary, no `call`
instruction appears in that loop at all).

Measured under Ubuntu WSL 2 with GCC 15.2.0, using two consecutive
`make benchmark` runs:

```text
run 1: add2 293.75 ms (1.47 ns/call); add_inline 280.80 ms (1.40 ns/call)
run 2: add2 280.72 ms (1.40 ns/call); add_inline 276.89 ms (1.38 ns/call)
```

(The inlined loop was slightly faster in both runs, but the difference
was only 0.02–0.07 ns/call. Both loops write to a `volatile` accumulator,
so this remains a small timing experiment rather than an isolated
measurement of call latency.)

The hardware-counter version is available as `make perf`. It was
attempted in the same environment, but `perf` is not installed, so no
retired-instruction or cycle counts are claimed here.

The floating-point experiment adds `add_fp2(double, double)` in
`src/math.S`. It receives operands in XMM0/XMM1, returns the sum in XMM0,
and is covered by the normal test target:

```text
ok: add_fp2 (XMM0/XMM1 floating-point register passing)
all tests passed
```

The alignment experiment also corrected `sum_three_locals`: its 40-byte
frame leaves RSP 16-byte aligned immediately before its nested `call`.
The opt-in `make misaligned` target deliberately calls libc `printf`
without that adjustment. On the measured Ubuntu WSL run it printed the
probe heading and then terminated with `Segmentation fault (core dumped)`
(status 139), demonstrating why the alignment rule matters. It is not
part of the normal test target.

## Results

The inlined version was slightly faster in both measured runs, which is
consistent with avoiding CALL/RET, but the small and variable gap means
the benchmark does not isolate that overhead cleanly. The volatile
accumulator creates a serial load/store dependency in both loops, so
hardware counters would be the appropriate next measurement; they were
unavailable in this environment. The floating-point test and the
misalignment probe provide direct ABI evidence independent of timing.

## What I learned

I expected the inlined loop to win easily, and the measurements were
consistent with that expectation. The small gap was a useful reminder
that a volatile accumulator can dominate a microbenchmark, so the
result should not be presented as a precise CALL/RET cost without
hardware-counter data.

## Limitations

- x86-64 System V only — no ARM64 (AAPCS64) comparison, though the
  underlying *concepts* (argument registers, callee-saved set, stack
  alignment) carry over with different register names and rules.
- The floating-point coverage is limited to two scalar `double`
  arguments; vector types and wider XMM register usage are not shown.
- `perf stat` could not be run because `perf` is not installed in the
  measured WSL environment.
- The misalignment probe intentionally crashes on this libc build and
  must remain opt-in; it is not sanitizer-checked or part of normal
  tests.
