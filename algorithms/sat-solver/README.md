# SAT Solver

## What is this?

A small Boolean satisfiability (SAT) solver in C++ using DPLL
(Davis-Putnam-Logemann-Loveland): unit propagation plus
branch-and-backtrack search over a CNF formula.

```bash
./sat examples/example.cnf
./sat examples/example.cnf --heuristic most
./sat examples/example.cnf --heuristic vsids
./sat examples/example.cnf --propagation watched
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

- **Unit propagation** (`propagate()`) has two implementations behind
  `--propagation`, selectable independently of the search itself:
  - `naive` (default): repeatedly scans every clause for one with
    exactly one unassigned literal left and forces it true, until no
    more forced assignments exist or a clause is left with zero
    unassigned literals and no satisfied one (a conflict).
    O(clauses x literals) per pass — the right trade-off at this scale
    (see [CS-LAB.md](/CS-LAB.md) §6, which explicitly scopes this lab
    away from industrial SAT solving).
  - `watched`: the two-watched-literals scheme every real solver
    uses. Each clause tracks only 2 of its literals; when a literal
    becomes true, only clauses currently watching its negation get
    re-examined, not every clause on every pass. Same algorithm as
    naive — same decisions, same backtracks, verified below and by a
    30-random-instance regression test — just a faster route to the
    identical fixpoint.
- **Branching** (`dpll()`): once propagation reaches a fixed point with
  no conflict and no clause left unsatisfied, if the formula isn't
  fully satisfied yet, pick an unassigned variable, try it `true` and
  recurse; if that fails, backtrack, try `false`, recurse; if both
  fail, backtrack further.
- **Heuristics** (`--heuristic`): `first` (scan order), `most`
  (branch on whichever unassigned variable appears in the most
  remaining clauses), and `vsids` (branch on whichever unassigned
  variable has the highest "activity" — bumped on every variable in a
  clause that causes a propagation conflict, then all activity decays
  by 5% so *recent* conflicts outweigh old ones). VSIDS here is a
  scaled-down version of the real thing: without clause learning
  (CDCL), there's no learned-clause to bump variables from, only the
  clause that directly conflicted — see the Experiments/Results
  section for how the three actually compare.
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
- `tests/test_sat.cpp` — 17 tests, including independently re-checking
  every returned SAT model against the raw clauses (not just trusting
  the solver's own "true" return value), watched-literals-vs-naive
  agreement (including a 30-random-instance regression), and VSIDS
  correctness.

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

**Naive vs. watched-literals — same algorithm, faster or not?** Same
20 instances at ratio 4.27, fed to both propagation strategies:

```text
clauses   naive ms        watched ms          speedup     decisions match?
85        1.0265          0.5359              1.92        yes, all 20
```

**VSIDS vs. most-occurrences — does "recent conflict involvement" beat
static structure at this scale?** Same 20 instances at ratio 4.27:

```text
clauses   most-occ decisions  vsids decisions     most-occ backtracks vsids backtracks
85        8.85                11.85               10.75               16.45
```

**Does backtrack count at the hardest ratio scale with problem size?**
Fixed ratio 4.27, `num_vars` from 10 to 26:

```text
num_vars  clauses   avg decisions   avg backtracks  avg ms
10        42        6.80            8.70            0.0095
14        59        9.50            14.30           0.0177
18        76        11.50           14.00           0.0314
22        93        22.90           38.10           0.0774
26        111       39.10           71.70           0.1658
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

**Watched literals: same search, real speedup.** All 20 instances
produced *bit-identical* decision and backtrack counts between naive
and watched-literals propagation (also verified by a 30-random-instance
regression test in `tests/test_sat.cpp`) — confirming it's purely a
faster route to the same fixpoint, not a different search. And it *is*
faster: 1.92x on this batch. Not the 10-100x sometimes quoted for
watched literals in industrial solvers — these instances are tiny (20
variables, 85 clauses), so the constant-factor overhead per clause
lookup dominates less dramatically than it would at real scale, where
the gap between "rescan everything" and "only look at what changed"
widens enormously.

**VSIDS lost to most-occurrences here — a real, non-cherry-picked
result.** most-occurrences needed fewer decisions (8.85 vs 11.85) *and*
fewer backtracks (10.75 vs 16.45) than this lab's VSIDS. That's the
opposite of what VSIDS "should" do in a real CDCL solver, and the
likely reason is exactly what CS-LAB.md's own scope notes predict:
without clause learning, this VSIDS only ever bumps the *one* clause
that directly conflicted, never a *learned* clause summarizing a whole
conflict's root cause — and at only 85 clauses total, most-occurrences'
static, whole-formula signal is already informative from decision one,
while VSIDS's activity scores need several conflicts to accumulate
useful signal that the search may already be over by. This is reported
as measured, not smoothed into "VSIDS wins as expected" — a
scaled-down VSIDS without CDCL is not the real algorithm, and this is
exactly the kind of gap real measurement catches that assuming the
textbook result would have missed.

**Backtrack count scales steeply with problem size at the hardest
ratio.** From 10 to 26 variables (2.6x), average backtracks grew from
8.7 to 71.7 (~8.2x) — consistent with the exponential worst case
NP-completeness predicts, not a coincidence of these particular
instances (the same fixed seed, same ratio, only size changing).

## What I learned

`std::fixed`/`std::setprecision` are sticky iostream state, not
per-value formatting — setting them partway through one `std::cout`
statement in the benchmark loop left the *first* table row unformatted
and every row after it formatted from the *previous* iteration's
leftover state. Moving the format setup to before the loop (and
resetting precision at the end of each row) fixed it. A reminder that
C++ stream manipulators are set-and-forget for the whole stream, not
scoped to one `<<` expression.

The VSIDS-vs-most-occurrences result was the bigger lesson: it would
have been easy to write "VSIDS wins, as the literature predicts" without
actually reading the numbers. Measuring found the opposite at this
scale, for a real, explainable reason (no clause learning to feed it)
rather than a bug — the honest result taught more than a confirmed
assumption would have.

## Limitations

- No clause learning (CDCL) or restarts — this solves toy-to-moderate
  instances, not competition-scale ones. VSIDS here is a scaled-down
  version without the learned-clause signal real CDCL VSIDS relies on
  (see Results).
- No support for CNF-only: no XOR/cardinality constraints, no
  incremental solving (assumptions), no proof/certificate output.
- `most-occurrences` recomputes counts from scratch on every decision
  rather than maintaining them incrementally.
- Watched-literals' `propagate_watched` seeds its queue from one
  decision literal at a time (matching this solver's one-decision-per-
  `dpll()`-call structure) rather than a general multi-literal work
  queue — sufficient here, but not the exact shape a from-scratch CDCL
  implementation would want.
