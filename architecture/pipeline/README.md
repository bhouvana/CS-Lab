# Pipeline Simulator

## What is this?

A classic 5-stage RISC pipeline (IF ID EX MEM WB) simulator in Python:
given a short program, it prints a per-cycle occupancy table and
measures how many stall cycles data hazards cost, with and without
forwarding.

```bash
python3 src/pipeline.py examples/program.asm
```

## Why does it matter?

"CPI" (cycles per instruction) is the number that ties instruction-set
design to real performance, and pipelining is *why* CPI can be close
to 1 instead of 5. But pipelining only works cleanly when instructions
are independent — data hazards force stalls, and forwarding
(bypassing a result straight from one stage to another instead of
waiting for it to reach the register file) is the single most
important technique for getting CPI back down. This lab makes both the
stalls and the fix visible cycle-by-cycle.

## Concept

```text
instructions (ADD/SUB/MUL/LOAD)
         |
         v
     IF ID EX MEM WB          one stage per cycle, one instruction issued per cycle
         |
         v
cycle table + stalls (RAW hazards) + CPI
```

```text
Cycle  1 2 3 4 5 6
ADD    F D E M W
SUB      F D E M W
LOAD       F D E M W
```

## How it works

- **Only RAW hazards are simulated** — see Limitations for why WAR/WAW
  cannot actually occur in this design at all, not just "aren't
  implemented."
- **Stalling** happens by holding an instruction in the ID stage
  (repeating "D" in its row) until its source registers are ready;
  that backpressure also holds earlier not-yet-issued instructions in
  IF, exactly as it would on real hardware.
- **Without forwarding**, a register is ready the cycle *after* its
  producer's WB stage — a consumer must wait for the full round trip
  through the register file.
- **With forwarding**, an ALU producer's result is ready the cycle
  after its *EX* stage (bypassed straight to a dependent instruction's
  EX), but a **LOAD** producer's result is only ready after its *MEM*
  stage (data comes back from memory, not the ALU) — this is the
  classic load-use hazard, and it costs one stall cycle even with full
  forwarding hardware.
- Stage-occupancy is computed per-instruction as `(start_cycle,
  next_stage_start_cycle)` ranges — this naturally prints repeated
  letters for a stalled stage without needing separate stall-tracking
  logic.

## Implementation

- `src/pipeline.py` — `Instruction` (parses `ADD Rd, Rs1, Rs2` /
  `LOAD Rd, offset(Rs)`), `simulate()` (the core algorithm), cycle
  table formatting, and the CLI.
- `src/bench.py` — the forwarding, control-hazard, superscalar, and
  forwarding-path experiments.
- `tests/test_pipeline.py` — 9 tests, including the exact stall counts
  for a zero-hazard program, an ALU RAW hazard with/without forwarding,
  and the load-use hazard.

## Example

```text
$ python3 src/pipeline.py examples/program.asm
=== WITH forwarding ===

Cycle   1  2  3  4  5  6  7  8  9
ADD     F  D  E  M  W
SUB        F  D  E  M  W
LOAD          F  D  E  M  W
MUL              F  D  D  E  M  W

instructions: 4  cycles: 9  stalls: 1  CPI: 2.25

=== WITHOUT forwarding ===

Cycle   1  2  3  4  5  6  7  8  9 10 11 12
ADD     F  D  E  M  W
SUB        F  D  D  D  E  M  W
LOAD          F  F  F  D  E  M  W
MUL                    F  D  D  D  E  M  W

instructions: 4  cycles: 12  stalls: 4  CPI: 3.00
```

(`examples/program.asm`: SUB needs ADD's result; MUL needs LOAD's
result, a load-use hazard, plus ADD's, already resolved by then.)

## Experiments

**How much does forwarding reduce stalls, and does it depend on
hazard density?** `src/bench.py` runs 50-instruction programs with
different dependency patterns: fully independent, 1-in-4 and 1-in-2
instructions depending on the previous one, a fully dependent chain,
and alternating load/use pairs.

Real output from `make benchmark`:

```text
pattern                 stalls (no fwd) stalls (fwd)  CPI (no fwd)  CPI (fwd)   reduction
independent (0%)        0               0             1.08          1.08        0.0%
mixed (1-in-4)          24              0             1.56          1.08        100.0%
mixed (1-in-2)          48              0             2.04          1.08        100.0%
dependent chain (100%)  98              0             3.04          1.08        100.0%
load-use pairs          50              25            2.08          1.58        50.0%
```

**1. Stall-on-branch control hazard.** A `BEQ` resolves in EX and holds
younger fetch until that cycle:

```text
branch EX cycle: 3  younger IF cycle: 4  cycles: 8  stalls: 2
```

**2. Two-issue in-order pipeline.** The benchmark compares the existing
single-issue forwarding model with a two-issue scheduler:

```text
pattern                 single cycles   dual cycles   single CPI    dual CPI
independent (0%)        54              29            1.08          0.58
dependent chain (100%)  54              54            1.08          1.08
load-use pairs          79              55            1.58          1.10
```

**3. Forwarding-path ablation.** Removing EX/MEM forwarding and keeping
only MEM/WB forwarding gives the following additional stalls:

```text
pattern                 full stalls     MEM/WB stalls   regression
dependent chain         0               98              98
load-use pairs          25              50              25
```

## Results

Forwarding eliminates **100% of ALU-to-ALU RAW hazard stalls**,
regardless of how dense they are — even a fully dependent 50-instruction
chain (every instruction needs the previous one's result) needs *zero*
stall cycles with forwarding, because EX/MEM bypassing delivers the
result exactly one cycle after it's produced, which is exactly when
the next instruction's EX stage needs it in the pipeline's normal,
unstalled cadence. The one pattern forwarding *doesn't* fully fix is
load-use pairs: forwarding still cuts their stalls in half (50 -> 25
over 25 pairs, i.e. from 2 down to 1 stall cycle per pair), but the
last cycle is irreducible — a load's data isn't available until after
MEM, one stage later than an ALU result, no matter how good the
bypass network is.

## What I learned

I expected forwarding's benefit to fade as hazards got denser (more
chances for the *previous* forwarding to not have finished yet), but
the dependent-chain result shows that's not how it works for
single-cycle ALU ops: since forwarding only needs the result exactly
one cycle after production, and the pipeline's natural unstalled
spacing between consecutive instructions is exactly one cycle, an
arbitrarily long chain of back-to-back ALU dependencies needs zero
extra stalls. Density stops mattering once the timing lines up exactly.

The branch model makes the control cost explicit: resolving in EX delays
the next fetch and adds two end-to-end stall cycles to the two-instruction
trace. Two-issue helps independent instructions but cannot accelerate a
dependency chain. Removing EX/MEM forwarding adds 98 stalls to the ALU
chain and 25 to load-use pairs, confirming that bypass path's role.

## Limitations

- **WAR and WAW hazards are not just unimplemented — they cannot occur
  here.** This is a single-issue, in-order pipeline: every instruction
  reads its registers (ID) and writes its result (WB) in strict
  program order, so a later instruction can never write a register
  before an earlier one has read the old value (WAR) or before an
  earlier one has written its own value (WAW). Those hazards require
  out-of-order execution or multiple in-flight writers per register —
  out of scope for this lab (see `CS-LAB.md` §48: that's Aegis-X86
  territory, not CS-Lab's).
- Structural hazards (e.g. multiple instructions needing the same
  functional unit) aren't modeled — every stage is assumed to always
  be available once an instruction is ready to enter it (subject only
  to the one-instruction-per-stage constraint already enforced).
- The branch, two-issue, and MEM/WB-only models are benchmark-focused;
  the core remains a compact single-issue timing model.
- This is a simulator, not RTL — no register file, no ALU, no memory
  actually modeled, only cycle timing.

