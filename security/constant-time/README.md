# Constant-Time Compare

**Educational side-channel experiment.** A simple benchmark like this
one does not, by itself, prove anything is cryptographically secure —
see Limitations.

## What is this?

Two byte-comparison functions, `insecure_compare()` (early-exit) and
`constant_time_compare()` (always examines every byte), benchmarked
against each other to show that the *time* a naive comparison takes
can leak *where* two secrets first differ.

```bash
./compare_demo
```

## Why does it matter?

`if (memcmp(user_input, secret_token) == 0)` looks harmless. It isn't:
an attacker who can measure response time precisely enough (over many,
many repeated guesses) can use exactly the timing gap this lab
measures to recover a secret one byte at a time, without ever seeing
it — this is a real, historically exploited class of vulnerability
(e.g. timing attacks against HMAC/token comparison in web frameworks).

## Concept

```text
two byte buffers
      |
      v
 comparison            two implementations, same correctness, different timing behavior
      |
      v
insecure_compare()          -> time depends on WHERE the first mismatch is
constant_time_compare()     -> time does not depend on that
```

## How it works

- **`insecure_compare`** returns the instant it finds a mismatched
  byte — a completely natural, everyday way to write a comparison
  loop, and exactly the problem: an attacker who can time responses
  can tell "the first N bytes of my guess matched" from "the
  comparison took proportionally longer."
- **`constant_time_compare`** never branches on the comparison result.
  It XORs every byte pair and OR-accumulates the differences into one
  value, checked against zero only once, at the very end — so every
  call does the same amount of work regardless of where (or whether) a
  mismatch exists.
- **Built at `-O0` on purpose.** An optimizing compiler is free to
  notice `constant_time_compare`'s loop has no early exit and
  vectorize or unroll it — good for real-world use, but it would blur
  this lab's side-by-side timing comparison. Real production code
  needs additional care (volatile/compiler-barrier tricks or a
  language-level primitive) to keep this property under optimization;
  see Limitations.

## Implementation

- `include/compare.h` / `src/compare.c` — both comparison functions.
- `src/main.c` — a correctness demo (both functions must always agree
  on *whether* two buffers match — only their *timing* differs).
- `src/bench.c` — the timing experiment.
- `tests/test_compare.c` — 7 tests, including that both functions
  agree at every single-byte mismatch position across an 8-byte buffer.

## Example

```text
$ ./compare_demo
equal                insecure=1  constant_time=1
differ at byte 0     insecure=0  constant_time=0
differ at last byte  insecure=0  constant_time=0
```

## Experiments

**Does comparison time leak the mismatch position?** `src/bench.c`
fixes a 32-byte secret and, for guesses matching it in the first 0,
4, 8, ..., 32 bytes, times 2,000,000 calls to each function.

Real output from `make benchmark`:

```text
match_len       insecure (ms)   constant-time (ms)
0               0.00            151.00
4               17.00           111.00
8               33.00           109.00
12              48.00           112.00
16              66.00           113.00
20              100.00          110.00
24              88.00           134.00
28              96.00           132.00
31              118.00          117.00
32              118.00          117.00
```

## Results

`insecure_compare`'s time climbs steadily from 0ms (mismatch at byte 0
— exits almost immediately) to ~118ms (full 32-byte match — no early
exit possible) as the matching prefix grows — a direct, visible timing
leak of exactly how many bytes of a guess are correct so far.
`constant_time_compare`'s time stays in a noisy but non-trending band
(109-151ms) regardless of match length — the "noise" is real system
jitter (scheduler, cache effects), not a signal correlated with the
mismatch position the way `insecure_compare`'s clear upward trend is.
This is the side channel made visible: an attacker probing
`insecure_compare` byte-by-byte, discarding guesses that don't
increase the response time, can recover a secret in `O(secret_length
x alphabet_size)` guesses instead of `O(alphabet_size^secret_length)`.

## What I learned

The difference is only clearly visible when averaged over millions of
calls — a single comparison's timing difference is a few nanoseconds
at most, far smaller than normal system noise (scheduler jitter, cache
state, clock resolution). Real timing attacks against network services
need exactly this same statistical averaging, often over thousands of
requests per guessed byte, which is *why* they're harder to pull off
remotely than locally, but not why they're impossible — TLS timing
attacks recovering secrets over a real network, with all its added
noise, are a documented category of real attack.

## Limitations

- **This benchmark does not prove security.** It shows a real,
  measurable timing difference under controlled, noise-averaged
  conditions on one machine — it says nothing about whether that
  difference is exploitable over a real network, through a real
  service, against a real attacker. Concluding "constant_time_compare
  is secure" from this benchmark alone would be exactly the kind of
  overclaim CS-LAB.md §31 rules out.
- **Compiler optimization can undo constant-time guarantees.** At
  higher optimization levels, a sufficiently clever compiler could
  theoretically transform even branch-free code in ways that
  reintroduce timing variance; production constant-time code typically
  needs compiler barriers or hardware intrinsics to guarantee this
  doesn't happen, which this lab doesn't implement.
- **CPU-level timing channels** (cache timing, branch predictor state,
  speculative execution) are a much larger, harder problem this lab
  doesn't touch at all — this lab is about one specific, easy-to-fix,
  application-level mistake, not the full space of timing side channels.

## Further experiments

- Rebuild at `-O2`/`-O3` and re-run the benchmark to see whether
  optimization narrows or erases the gap.
- Measure the same comparison over an actual local TCP round-trip
  (reusing `networking/tcp-chat`'s socket code) to see how much network
  jitter narrows the observable signal compared to an in-process call.
- Implement a real HMAC-based token comparison (naive vs.
  constant-time) and repeat the experiment on token-length secrets.
