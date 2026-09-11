# SAT Solver

## What is this?

A small Boolean satisfiability (SAT) solver in C++ using DPLL
(Davis-Putnam-Logemann-Loveland): unit propagation plus
branch-and-backtrack search over a CNF formula.

```bash
./sat examples/example.cnf
./sat examples/example.cnf --heuristic most
```

## Why does it matter?

SAT is the canonical NP-complete problem — the first one proven
NP-complete (Cook-Levin, 1971), and the one every other NP-complete
problem reduces to. Every practical SAT solver (used in hardware
verification, scheduling, cryptanalysis) is still, at its core, DPLL
with decades of engineering on top. This lab is the "at its core" part.

## Concept

```text
CNF formula (variables, literals, clauses)
         |
         v
      DPLL
   /            \
unit propagation   branch on a variable, try true, backtrack, try false
         |
         v
SAT (with a model) or UNSAT
```

Input is DIMACS CNF (the standard SAT competition format):

```text
c a comment
p cnf 3 3
1 2 0
-1 3 0
-3 2 0
```

`p cnf 3 3` declares 3 variables and 3 clauses; each subsequent line is
a clause (OR of the listed literals, `-v` meaning NOT variable v)
terminated by `0`. The example above encodes
`(x1 OR x2) AND (NOT x1 OR x3) AND (NOT x3 OR x2)`.

## How it works

- **Unit propagation** (`propagate()`): repeatedly scans every clause
  for one with exactly one unassigned literal left and forces it true,
  until no more forced assignments exist or a clause is left with zero
  unassigned literals and no satisfied one (a conflict). This is a
  naive O(clauses x literals)-per-pass scan, not the watched-literals
  technique real solvers use — the right trade-off at this scale (see
  [CS-LAB.md](/CS-LAB.md) §6, which explicitly scopes this lab away
  from industrial SAT solving).
- **Branching** (`dpll()`): once propagation reaches a fixed point with
  no conflict and no clause left unsatisfied, if the formula isn't
  fully satisfied yet, pick an unassigned variable, try it `true` and
  recurse; if that fails, backtrack, try `false`, recurse; if both
  fail, backtrack further.
- **Heuristics**: `first-unassigned` (scan order) and
  `most-occurrences` (branch on whichever unassigned variable appears
  in the most remaining clauses) — see the Experiments/Results section
  for how they actually compare.
- **Complete models**: a variable that never constrains satisfiability
  (a "don't care") can be left unassigned by DPLL even when the
  formula is SAT; `solve()` fixes any such variables to `true` before
  returning, so callers always get a full model.

## Implementation

- `include/sat.hpp` — `Solver`, `SolveStats`, `CnfFormula`.
- `src/sat.cpp` — the DPLL algorithm itself.
- `src/dimacs.cpp` — DIMACS CNF file parser.
- `src/main.cpp` — CLI.
- `src/bench.cpp` — the phase-transition experiment.
- `tests/test_sat.cpp` — 9 tests, including independently re-checking
  every returned SAT model against the raw clauses (not just trusting
  the solver's own "true" return value).

## Example

```text
$ ./sat examples/example.cnf
SAT

x1 = true
x2 = true
x3 = true

--- stats ---
variables:         3
clauses:           3
decisions:         1
backtracks:        0
unit propagations: 2
runtime:           0.0012 ms

$ ./sat examples/unsat.cnf
UNSAT
...
```

## Experiments

**How does clause count affect runtime?** Random 3-SAT has a famous
"phase transition": at low clauses-to-variables ratios almost every
formula is satisfiable and easy; at high ratios almost every formula
is unsatisfiable and also easy (contradictions surface fast); right at
the critical ratio (~4.27 for 3-SAT), formulas are hardest to decide
either way. `src/bench.cpp` generates 20 random 3-CNF instances per
ratio (20 variables, fixed seed) and averages decisions/backtracks/
runtime.

Real output from `make benchmark`:

```text
ratio     clauses   %SAT      avg decisions avg backtracks  avg ms
2.00      40        100.00    9.65          0.55            0.0087
3.00      60        100.00    11.15         7.85            0.0203
3.50      70        95.00     13.80         14.30           0.0327
4.00      80        85.00     12.00         15.50           0.0367
4.27      85        65.00     22.35         37.85           0.0782
4.50      90        75.00     12.55         16.70           0.0413
5.00      100       30.00     15.80         28.40           0.0534
6.00      120       5.00      12.60         24.40           0.0534
8.00      160       0.00      7.35          14.70           0.0384
```

## Results

The satisfiability rate drops from 100% to 0% almost exactly where the
literature predicts (crossing 50% between ratio 4.5 and 5.0, close to
the theoretical ~4.267 threshold for 3-SAT). More strikingly, **both
avg-backtracks and avg-runtime peak at ratio 4.27** — 37.85 backtracks
and 0.078ms, the highest of any row in the table, roughly double the
neighboring ratios on either side. This is the phase transition made
visible: formulas near the satisfiability threshold are measurably the
hardest for DPLL to resolve, exactly where existing SAT theory says
they should be.

## What I learned

`std::fixed`/`std::setprecision` are sticky iostream state, not
per-value formatting — setting them partway through one `std::cout`
statement in the benchmark loop left the *first* table row unformatted
and every row after it formatted from the *previous* iteration's
leftover state. Moving the format setup to before the loop (and
resetting precision at the end of each row) fixed it. A reminder that
C++ stream manipulators are set-and-forget for the whole stream, not
scoped to one `<<` expression.

## Limitations

- Naive unit propagation (full clause rescans), no watched literals,
  no clause learning (CDCL), no restarts — this solves toy-to-moderate
  instances, not competition-scale ones.
- No support for CNF-only: no XOR/cardinality constraints, no
  incremental solving (assumptions), no proof/certificate output.
- `most-occurrences` recomputes counts from scratch on every decision
  rather than maintaining them incrementally.

## Further experiments

- Add a watched-literals implementation and compare decision/backtrack
  counts (should be identical — it's a performance optimization, not a
  different algorithm) against wall-clock time (should be much faster).
- Try a VSIDS-style heuristic (weight by recent conflict involvement)
  against `most-occurrences` on the same phase-transition instances.
- Sweep `num_vars` at a fixed ratio near 4.27 to see how backtrack
  count scales with problem size at the hardest ratio.
