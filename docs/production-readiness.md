# Production Readiness Report

CS-Lab Hardened pass, 2026-09-11. This is the closing report the hardening
directive (`CS-LAB-HARDENED.md`, if kept, or the conversation that produced
this pass) asked for.

## What changed?

Starting point: 20 working labs, real measured experiments, 7 previously
discovered and fixed bugs, but no CI, no sanitizer wiring at the root level,
no consolidated audit, and (found during this pass) a handful of real gaps:
two files committed by accident, a Windows-only clock() bug that had been
fixed in some benchmarks but not all, a Makefile that only worked with
`python` and not `python3`, a gitignore that didn't cover this repo's own
Linux build outputs, and a silent-failure path in the bytecode VM's opcode
dispatch. All of those are now fixed -- see `docs/engineering-audit.md` for
the itemized list and `docs/regressions.md` for how each one is now
permanently guarded by a test.

Full diff is 6 commits (see `git log`): Rust formatting/lint, sanitizer
wiring across 6 more labs plus two portability fixes, root Makefile tooling,
the bytecode-VM opcode fix and fuzz test, a CI workflow, and this
documentation.

## What was hardened?

- 9 C/C++ labs (the CS-LAB.md §12 priority list) now have a `test-asan`
  Makefile target running ASan+UBSan; all 9 pass clean.
- The bytecode VM rejects out-of-range opcodes instead of silently
  skipping them (a real gap found while adding its fuzz test), and gained a
  20,000-case deterministic fuzz test.
- Root `make build`/`test`/`benchmark`/`clean` no longer hard-fail when
  `cargo` isn't on `PATH` -- they skip Rust labs explicitly instead.
- Root `make sanitize`/`format`/`lint`/`check` are new.
- All 4 Rust crates are `cargo fmt`-clean and `cargo clippy`-clean (one real
  warning fixed: `manual_div_ceil` in bloom-filter).
- Two real portability bugs fixed: `architecture/pipeline`'s Makefile
  hardcoding `python` (fails on stock Ubuntu), and `compression/huffman`'s
  benchmark using a single `clock()` reading (reads 0ms on native
  Windows/MinGW).
- `.gitignore` now covers Python bytecode cache and every lab's
  extensionless Linux/WSL build output.

## What tests were added?

- `compilers/bytecode-vm`: `test_invalid_opcode_invalid_case` (regression)
  and `test_random_programs_never_crash_fuzz` (20,000-program deterministic
  fuzz test, fixed-seed xorshift32, asserts `vm_run()` never returns
  anything but 0 or -1).
- No other new test files -- the audit's main finding was that the existing
  test suites (19 lab test files, 32 Rust tests + ~100 C/C++ assertions
  across the rest) were already substantially edge-case-covered: every lab
  already had invalid-input, boundary, and regression cases before this
  pass started. See `docs/engineering-audit.md`'s per-lab test counts.

## What real bugs are now permanently covered?

Eight, total -- the 7 discovered building the original 20 labs, plus one
found during this pass. Full table with fix location and test name:
`docs/regressions.md`.

## What sanitizers were used?

AddressSanitizer + UndefinedBehaviorSanitizer (`-fsanitize=address,undefined
-g`), via GCC 15.2.0 on WSL2 Ubuntu. No LeakSanitizer run separately (ASan's
`detect_leaks=1` default covers it, and was enabled during ad hoc
verification runs). Not run: native Windows/MinGW (no sanitizer runtime for
this MinGW build), and the 4 non-priority C/C++ labs (graph-algorithms,
cache, branch-predictor) plus the 3 assembly labs' `.S` files (not
ASan-instrumentable). See `docs/quality.md` footnote 2.

## What fuzzing was performed?

One real fuzz harness: `compilers/bytecode-vm`'s 20,000-case deterministic
random-program generator (see above). Every other parser/decoder in the
priority list (SAT's DIMACS parser, Huffman's compressed-stream decoder,
LZ77's token-stream decoder, tiny-language's lexer/parser) already had
deterministic malformed-input test cases from the original build (missing
files, bad magic bytes, truncated headers, corrupt non-multiple-of-4 token
streams, unknown mnemonics, unrecognized tokens) that exercise the same
"reject malformed input cleanly" property at lower cost than a full fuzz
loop. CS-LAB.md's own fuzzing section explicitly prefers deterministic
malformed-input generators over volume when a fuzzing engine isn't
available, and no libFuzzer/AFL setup exists in this environment.

## What platforms are supported?

- **Cross-platform (Windows + Linux, verified both):** the 4 Rust labs, the
  11 portable C/C++/Python labs. See `docs/reproducibility.md` for exact
  toolchain versions used on each side.
- **Linux/POSIX only, by design (verified on WSL2 Ubuntu):**
  `networking/tcp-chat` (POSIX sockets).
- **Linux x86-64 only, by design (verified on WSL2 Ubuntu):** the 3
  assembly labs -- hand-written code assumes the System V calling
  convention and real Linux syscall numbers; this isn't a portability nicety,
  it would produce silently wrong results under a different ABI, not a
  clean build failure.

All four Linux-only labs' Makefiles detect the platform and print an
explicit `skip:` line (exit 0) rather than failing when built elsewhere.

## What platforms are intentionally unsupported?

macOS was never targeted or tested (no machine available in this
environment); nothing in the source suggests it wouldn't work for the
portable labs, but that's an untested claim, not a verified one, and is
reported as such rather than implied. 32-bit x86, ARM, and other non-x86-64
architectures are out of scope for the assembly labs by design (they teach
one concrete ABI, not a portable abstraction over several).

## What benchmarks are reproducible?

All of them -- see `docs/reproducibility.md` for the exact commands. Every
benchmark that uses randomness seeds it explicitly (`docs/reproducibility.md`
lists each seed). Every benchmark that times sub-millisecond operations
batches iterations until a minimum elapsed time is reached, rather than
trusting a single `clock()` reading (this pass fixed the one file where that
pattern was missing). None of the numbers in `docs/experiments.md` were
re-measured or changed by this pass -- they were real when recorded and
nothing here contradicts them; this pass only fixed the *measurement
methodology* in the one place it was broken (huffman).

## What limitations remain?

- CI is written (`.github/workflows/ci.yml`) but has never actually run --
  this repository has no configured git remote in this environment. Every
  step was verified independently on the appropriate side (Windows for
  Rust, WSL for everything else), which is real evidence but not the same
  claim as "CI is green." See `docs/reproducibility.md`.
- Sanitizer coverage is 9 of ~15 eligible C/C++ labs (the CS-LAB.md §12
  priority list), not all of them. The remaining 6 are untested by
  sanitizers, not known-broken.
- No fuzz harness beyond bytecode-vm's. The other parsers rely on
  deterministic malformed-input test cases, which cover the same intent
  (per CS-LAB.md §11's own stated preference) but not the same breadth a
  real fuzzing engine would find over time.
- `clang-format` is documented (`.clang-format`) but not retroactively
  applied to the existing ~30 C/C++ source files -- see the root Makefile's
  comment on `format` for why forcing that here would be unrelated churn
  against already-reviewed, zero-warning code.
- No `cppcheck`/`clang-tidy` static analysis pass -- neither tool with a
  working compilation database was available in this environment. The
  `-Wall -Wextra -Wpedantic` warnings already required by every C/C++
  Makefile, and now verified at 0 across a full clean rebuild, are the
  static-analysis coverage that does exist.
- `os/shell` was never built. It's listed as "planned" in its own README
  and was correctly left alone -- building a new lab is out of scope for a
  hardening pass.
- macOS is untested (see above).

## What does "production-ready" mean for this repository?

Not that any of these 20 implementations are drop-in replacements for
mature industrial software -- none of them make that claim, and several
(SHA-256, constant-time compare) explicitly disclaim it in their own
READMEs. It means:

> Each implementation is engineered to a high standard appropriate to its
> educational scope, with explicit limitations, deterministic behavior,
> robust error handling, meaningful tests, reproducible experiments, and no
> known correctness or memory-safety defects under its supported operating
> conditions.

That standard is met for all 20 labs (19 implemented + `shell` correctly
documented as not implemented, rather than either built out of scope or
silently missing from the record).
