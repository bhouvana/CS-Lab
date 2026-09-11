# Branch Predictor

## What is this?

Five branch predictors — always-taken, always-not-taken, 1-bit, 2-bit
saturating counter, and gshare — evaluated against synthetic branch
outcome traces.

```bash
./branch examples/loop.trace
./branch --predictor gshare examples/loop.trace
```

## Why does it matter?

Modern CPUs are deeply pipelined and speculate past every conditional
branch before knowing which way it goes; a misprediction means
throwing away all that speculative work (a "pipeline flush," directly
connected to `architecture/pipeline` in this repo). Prediction
accuracy is a direct multiplier on real-world CPU performance — this
lab makes that abstract idea into a measurable number.

## Concept

```text
branch outcome trace (T/N sequence)
         |
         v
      predictor        guesses the NEXT outcome, then observes the real one
         |
         v
   correct / incorrect  ->  accuracy
```

## How it works

- **Always Taken / Always Not Taken**: no state, a fixed guess.
- **1-bit**: predicts whatever happened last time.
- **2-bit**: a saturating counter (0-3, >=2 predicts taken); flipping
  the prediction needs two consecutive outcomes in the new direction,
  so one "glitch" against an otherwise consistent trend costs one
  misprediction here instead of the two it costs the 1-bit predictor.
- **GShare**: an 8-bit global history register indexes a table of 256
  independent 2-bit counters, so the prediction depends on the recent
  *pattern* of outcomes, not just the single last one or a fixed
  direction. A real multi-branch gshare indexes with `history XOR PC`;
  this lab's trace format is one implicit branch stream (matching the
  project directive's plain `T T N T T T N N ...` input), so the index
  is the history register alone — equivalent to XOR-ing with a
  constant PC.

## Implementation

- `include/predictors.hpp` — all five predictors (each 5-15 lines) plus
  `evaluate_predictor()`.
- `include/patterns.hpp` / `src/patterns.cpp` — synthetic pattern
  generators and the trace file loader.
- `src/main.cpp` — CLI (single predictor, or a comparison table).
- `src/bench.cpp` — the cross-pattern experiment.
- `tests/test_predictors.cpp` — 9 tests, including the specific
  claims above (2-bit beats 1-bit on a loop pattern; gshare learns
  alternating; 1-bit is ~0% on alternating, its worst case).

## Example

```text
$ ./branch examples/loop.trace
Predictor            Accuracy
--------------------------------
Always Taken          90.0%
Always Not Taken      10.0%
1-bit                 82.5%
2-bit                 90.0%
GShare                90.0%
```

## Experiments

**Which predictor handles which pattern best?** `src/bench.cpp` runs
all five predictors against five synthetic patterns (2000 branches
each): always-taken, alternating, a 9-taken/1-not-taken loop, 50%
random, and 90%-biased-random.

Real output from `make benchmark`:

```text
Alternating (T N T N ...) (2000 branches)
Predictor            Accuracy
--------------------------------
Always Taken          50.0%
Always Not Taken      50.0%
1-bit                  0.1%
2-bit                 50.0%
GShare                99.8%

Loop (9 taken, 1 not-taken, repeating) (2000 branches)
Predictor            Accuracy
--------------------------------
Always Taken          90.0%
Always Not Taken      10.0%
1-bit                 80.0%
2-bit                 90.0%
GShare                90.0%

Random (50% taken) (2000 branches)
Predictor            Accuracy
--------------------------------
Always Taken          49.2%
Always Not Taken      50.8%
1-bit                 51.3%
2-bit                 51.3%
GShare                50.4%

Biased (90% taken) (2000 branches)
Predictor            Accuracy
--------------------------------
Always Taken          90.4%
Always Not Taken       9.6%
1-bit                 82.5%
2-bit                 89.6%
GShare                89.5%
```

(Always-taken and always-not-taken scores on the always-taken pattern
omitted above for space — see `docs/experiments.md` for the full run.)

## Results

Three findings, all from the real numbers above:

1. **Alternating is 1-bit's worst case and gshare's best case.** 1-bit
   predicts "whatever happened last," which on a strictly alternating
   sequence is *always exactly wrong* (0.1% — the 0.1% is the one lucky
   first guess). GShare, indexed by history rather than just the last
   outcome, learns the 2-cycle pattern almost perfectly (99.8%).
2. **2-bit genuinely beats 1-bit on the loop pattern** (90.0% vs.
   80.0%) — exactly the textbook justification for 2-bit counters:
   tolerating one glitch (the loop's single not-taken exit) without
   flipping the prediction.
3. **GShare does not dominate every pattern.** On the loop and biased
   patterns, GShare ties or very slightly trails the simpler 2-bit
   counter (90.0% vs 90.0%; 89.5% vs 89.6%). With only 2000 samples
   spread across 256 possible 8-bit histories, gshare's per-context
   counters see too few updates each to out-learn a single global
   2-bit counter on patterns that don't have gshare's kind of
   short-range correlation. This is a real, honest result — GShare's
   advantage is specific to patterns with learnable *history-dependent*
   structure (like alternating), not a universal win.

## What I learned

It would have been easy to only report the alternating-pattern result
(GShare's best case) and imply gshare is simply "the best predictor" —
but running the full pattern sweep showed it isn't, on two of the five
patterns tested. Measuring across a spread of realistic patterns
instead of one favorable case is what surfaced that.

## Limitations

- Single implicit branch stream — no per-PC state, so gshare's real
  strength (correlating *across different static branches* via shared
  global history) isn't exercised here, only its within-branch,
  history-pattern-learning behavior.
- No tournament/hybrid predictor (choosing between two predictors per
  branch), which is what real high-end CPUs actually use.
- Fixed 8-bit history register / 256-entry table for gshare — not
  swept as a parameter.

## Further experiments

- Sweep gshare's history-register width and find where accuracy peaks
  for the loop pattern specifically (period 10 doesn't divide evenly
  into a power-of-two history length).
- Extend the trace format to carry a PC per branch and implement a
  true multi-branch gshare (`history XOR PC`), then construct a trace
  where per-branch correlation actually matters.
- Add a simple tournament predictor (meta-counter choosing between
  1-bit and gshare per branch) and compare it against both.
