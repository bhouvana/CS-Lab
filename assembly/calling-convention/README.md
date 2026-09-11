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
- `examples/add_equivalent.c` — the C source compared against `add2`
  via `make objdump`.
- `tests/test_calling_convention.c` — 5 tests, including the runtime
  callee-saved proof.

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

Real output from `make benchmark`:

```text
via add2 (real CALL/RET):            230.93 ms  (1.15 ns/call)
via add_inline (compiler-inlined):   278.77 ms  (1.39 ns/call)
```

(Both loops write to a `volatile` accumulator each iteration — this is
what stops the compiler from optimizing the whole loop away, and it
turns out to matter a lot; see Results.)

## Results

The real function call is **faster**, not slower — the opposite of
what the experiment set out to demonstrate, and reproducible across
repeated runs and with the two loops' order swapped (ruling out
warm-up bias). The likely explanation: both loops are actually
bottlenecked by store-to-load forwarding through the `volatile sink`
variable (the load of `sink` each iteration depends on the *previous*
iteration's store to it, a serial chain neither version can avoid).
`add2`'s own arithmetic has no memory dependency, so an out-of-order
CPU can compute it well ahead of when the result is needed, hiding the
`call`/`ret` cost entirely behind that unavoidable memory latency. The
inlined version's instruction sequence apparently schedules slightly
less favorably around that same bottleneck. Either way, the honest
takeaway isn't "calls are slower" — it's that at the nanosecond scale,
what dominates is rarely the thing you'd guess without measuring.

## What I learned

I expected the inlined loop to win easily and had a tidy explanation
ready ("no call/ret overhead"). Measuring first, instead of trusting
that expectation, is what caught it being backwards — and disassembling
both loops (confirming `add_inline` really was inlined, no `call`
present) was necessary to rule out "maybe it just didn't inline" before
accepting the surprising result as real.

## Limitations

- x86-64 System V only — no ARM64 (AAPCS64) comparison, though the
  underlying *concepts* (argument registers, callee-saved set, stack
  alignment) carry over with different register names and rules.
- Only integer/pointer arguments — no floating-point (XMM0-7) argument
  passing demonstrated.
- The benchmark's surprising result is explained by a plausible
  hypothesis (store-forwarding bottleneck hiding call latency), not
  confirmed with hardware performance counters (`perf stat`) — a
  genuine further-experiment candidate, not a settled explanation.

## Further experiments

- Re-run the benchmark under `perf stat` to see actual retired
  instructions/cycles per loop and test the store-forwarding
  hypothesis directly.
- Add a floating-point argument function (XMM0/XMM1 in, XMM0 out) to
  cover the other half of the System V argument-passing rules.
- Deliberately misalign the stack before a `call` inside
  `sum_three_locals` (e.g. push one extra byte's worth) and observe
  what breaks when calling a real libc function that uses SSE
  instructions requiring 16-byte alignment.
