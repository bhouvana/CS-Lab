# Engineering Audit

Baseline taken at the start of the CS-Lab Hardened pass (2026-09-11), before
and during hardening. Every cell below reflects a command actually run in
this repository -- see `docs/reproducibility.md` for exact commands and
toolchain versions. Findings from this audit that produced code changes are
listed under each table with a link to the commit; the change is not
described twice.

Legend: **PASS** verified working · **FIXED** was broken, now verified
working · **PARTIAL** works but with a documented gap · **N/A** doesn't apply
to this lab · **SKIP** correctly skips on unsupported platforms, not a
failure.

## Portable labs (build/test on Windows and Linux)

| Lab | Language | Build | Tests | Sanitizers | Fuzzing | Benchmark | Error handling |
|---|---|---|---|---|---|---|---|
| SAT Solver | C++ | PASS | PASS (9) | PASS (ASan+UBSan) | PARTIAL (deterministic malformed-input cases; no random corpus) | PASS (seeded, mt19937(42)) | PASS |
| Graph Algorithms | C++ | PASS | PASS | N/A (not in §12 priority list) | N/A | N/A (no bench.cpp) | PASS |
| Bloom Filter | Rust | PASS (fmt+clippy clean) | PASS (5) | N/A (Rust; memory safety is the language's job) | N/A | PASS | PASS (assert! on invalid params) |
| Tiny Language | Rust | PASS (fmt+clippy clean) | PASS (10) | N/A | N/A | N/A (interpreter demo, no bench) | PASS (line/col-free but message-carrying Result errors) |
| Bytecode VM | C | PASS | PASS (18, incl. new fuzz test) | **FIXED** (was clean; found + fixed a real gap, see below) | **FIXED** (added 20000-program deterministic fuzz test) | PASS (batched, MIN_MS-gated) | PASS |
| Huffman | C | PASS | PASS (7) | PASS (ASan+UBSan) | PARTIAL (deterministic malformed-input cases) | **FIXED** (clock() coarseness, see below) | PASS |
| LZ77 | C++ | PASS | PASS (9) | PASS (ASan+UBSan) | PARTIAL (deterministic malformed-input cases) | PASS | PASS |
| SHA-256 | C | PASS | PASS (6, incl. streamed==one-shot) | PASS (ASan+UBSan) | N/A (fixed-format input) | PASS (batched) | PASS |
| Cache Simulator | C++ | PASS | PASS (9) | N/A | N/A | N/A (trace-driven, no separate bench) | PASS |
| Branch Predictor | C++ | PASS | PASS (9) | N/A | N/A | PASS (seeded, mt19937(42)) | PASS |
| Pipeline Simulator | Python | PASS | PASS (9) | N/A (Python) | N/A | PASS | PASS |
| Garbage Collector | C | PASS | PASS (7, incl. 200k-deep chain) | PASS (ASan+UBSan) | N/A | PASS (batched) | N/A (internal API, not user input) |
| Tiny LSM | Rust | PASS (fmt+clippy clean) | PASS (9) | N/A | N/A | PASS | PASS (io::Result) |
| Constant-Time Compare | C | PASS | PASS (7) | PASS (ASan+UBSan) | N/A | PASS | N/A |
| Allocator | C | PASS | PASS (12) | PASS (ASan+UBSan) | N/A | PASS | PASS |

## Linux-only labs (build/test on Linux only; SKIP on Windows, not FAIL)

| Lab | Language | Build (Linux) | Tests (Linux) | Sanitizers | Benchmark | Windows behavior |
|---|---|---|---|---|---|---|
| TCP Chat | C | PASS | PASS (5, incl. SIGPIPE regression) | PASS (ASan+UBSan) | PASS (real clock_gettime/CLOCK_MONOTONIC) | SKIP (Makefile detects non-Linux, prints `skip:`) |
| Calling Convention | C/ASM | PASS | PASS (5) | N/A (hand ASM; ASan doesn't instrument `.S`) | PASS | SKIP |
| Stack Frames | C/ASM | PASS | PASS (4 + 1 end-to-end) | N/A | N/A | SKIP |
| Syscall Lab | C/ASM | PASS | PASS (5) | N/A | PASS | SKIP |
| Raft Simulator | Rust | PASS (fmt+clippy clean) | PASS (8) | N/A | N/A (deterministic tick-driven, no wall-clock bench) | PASS (Rust is cross-platform; this one just has no Linux-specific dependency either) |
| Shell | C | PASS | PASS (15, incl. SIGINT regression) | PASS (ASan+UBSan) | PASS (real clock_gettime/CLOCK_MONOTONIC) | SKIP (Makefile detects non-Linux, prints `skip:`) |

Raft is cross-platform (pure Rust, no Linux dependency) and is listed here
only because distributed systems fit next to networking conceptually --
`make build`/`test` run it on Windows too.

## Audit findings that produced changes (commit history has the full detail)

1. **Two `__pycache__/*.pyc` files were committed to git** despite a
   `.gitignore` that never listed them. Removed from tracking; `.gitignore`
   updated.
2. **`architecture/pipeline`'s Makefile hardcoded `PYTHON := python`.**
   Reproduced the real failure on stock Ubuntu/WSL (`make: python: No such
   file or directory` -- only `python3` exists there). Fixed with a
   `python3`-first, `python`-fallback `$(shell command -v ...)`.
3. **`compression/huffman`'s `bench.c` used a single `clock()` reading per
   file.** Reproduced 0.000ms readings on native Windows/MinGW (same class
   of bug as the already-fixed clock() coarseness in sha256/bytecode-vm/gc
   bench.c, just missed in this one file). Fixed with the same
   repeat-until-`MIN_MS` pattern already used elsewhere.
4. **`compilers/bytecode-vm`'s `vm_run()` dispatch had no `default` case.**
   An out-of-range `Opcode` (unreachable via the text assembler, but
   `Program`/`Instruction` are a public, directly-constructible API) was
   silently skipped rather than rejected. Fixed, with a regression test and
   a 20000-program deterministic fuzz test.
5. **Extensionless Linux/WSL build binaries were untracked but not
   gitignored** (the existing `*.exe`/`*.out`/`*.o` patterns never matched
   them) -- a `git add -A` on this Windows-primary repo would have staged
   ~35 binaries the moment someone built under WSL. Added explicit entries
   generated from each lab's own `clean:` target.
6. **One real `clippy::manual_div_ceil` warning** in bloom-filter's
   `BloomFilter::new`. Fixed (`num_bits.div_ceil(64)`).
7. **All 4 Rust crates failed `cargo fmt --check`** (no `rustfmt.toml`, so
   rustfmt's 100-column default disagreed with this codebase's ~110-120
   column lines). Added `rustfmt.toml` (`max_width = 120`, matching the
   existing style) rather than reformatting to an arbitrary default, then
   ran `cargo fmt`.
8. **6 of the 9 sanitizer-priority labs (CS-LAB.md §12) had no `test-asan`
   Makefile target** (only allocator, garbage-collector, and constant-time
   did). Added the same target, in the same style, to sat-solver,
   bytecode-vm, huffman, lz77, sha256, and tcp-chat. All 9 now run clean
   under ASan+UBSan.
9. **No root `make sanitize`/`format`/`lint`/`check`**, and root
   `build`/`test`/`benchmark` hard-failed at the first Rust lab whenever
   `cargo` wasn't on `PATH`. Added the four targets; `build`/`test`/
   `benchmark`/`clean` now print `skip: <lab> -- cargo not found on PATH`
   and continue instead of aborting.
10. **No CI.** Added a single-job GitHub Actions workflow (see
    `docs/reproducibility.md` for why it hasn't been run here -- no
    configured remote in this environment).
11. **`os/shell` was scoped but never built** ("planned, not yet
    implemented" since the original Phase 6). Built to the same standard
    as the other 19 (fork/execvp/waitpid, 3 builtins, quote-aware
    tokenizing, correct `SIGINT` handling, 15 tests, ASan+UBSan clean,
    a real fork+exec+wait throughput benchmark) once the repo's owner
    asked for all 20 rather than 19 -- see its own README for the design
    and `docs/regressions.md`-adjacent note below for a bug caught
    *during* construction, before it ever shipped broken.

While building it, `os/shell/src/bench.c` hit the exact
buffered-stdout-duplicated-into-`fork()`'d-child bug
`networking/tcp-chat`'s `bench.c` already documents (a `fork()` copies
still-unflushed `stdout` content into the child; something in the
child's teardown flushes that inherited copy a second time into the
shared descriptor). Recognized immediately from the existing comment in
tcp-chat's bench.c and fixed the same way (`fflush(stdout)` right before
`fork()`) before the file was ever committed in a broken state -- not
added to `docs/regressions.md` since there's no shipped-broken state and
no dedicated regression test guarding it, just the fix and an explanation
in the shell lab's own README under "What I learned."

## What this audit did NOT do

- Did not re-derive or re-verify every benchmark number already in
  `docs/experiments.md` -- those were real when measured and are unchanged
  by this pass, except where a benchmark's *measurement method* itself was
  the finding (huffman, above).
- Did not add `test-asan` to graph-algorithms, cache, branch-predictor, or
  the 3 non-shell assembly labs -- not on CS-LAB.md §12's priority list,
  and the assembly labs' hand-written `.S` files aren't ASan-instrumentable
  anyway (only their C callers would be, which adds little). `os/shell`
  got `test-asan` anyway since it's plain C and cost nothing extra.
- Did not add a fuzz harness to every parser -- bytecode-vm got one
  (highest-value target: a public, embeddable API executing structured
  input). SAT/Huffman/LZ77/tiny-language already have deterministic
  malformed-input test cases covering the same intent at lower cost; see
  CS-LAB.md §11's own preference for "discovering malformed-input bugs"
  over volume.
