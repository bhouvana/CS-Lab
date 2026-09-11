# Reproducibility

## What was actually run, where, and when

Every build/test/sanitize/benchmark claim in this repository's docs was
produced on one of these two environments during the CS-Lab Hardened pass
(2026-09-11):

| | Windows (native) | WSL2 Ubuntu |
|---|---|---|
| OS | Windows 11 | Ubuntu, kernel `6.6.87.2-microsoft-standard-WSL2` |
| C compiler | `gcc.exe`/`g++.exe` 8.1.0 (MinGW-W64, x86_64-posix-seh) | `gcc`/`g++` 15.2.0 (Ubuntu 15.2.0-16ubuntu1) |
| Rust | `rustc`/`cargo` 1.96.0 | not installed |
| Python | (not needed natively; `pipeline`'s Makefile falls back to it) | Python 3.14.4 |
| `make` | none (only `mingw32-make`, unused) | GNU Make 4.4.1 |
| Sanitizers (ASan/UBSan) | not available (no runtime libs for this MinGW build) | available, used |

This split is why the repo's labs fall into two groups (see
`docs/engineering-audit.md`):

- **Rust labs** (`bloom-filter`, `tiny-language`, `tiny-lsm`,
  `raft-simulator`) build and test on Windows, via `cargo` directly in each
  lab directory. Root `make build`/`test`/etc. also work if run somewhere
  `cargo` is on `PATH`.
- **C/C++/Python labs** (everything else) build and test on WSL Ubuntu, via
  each lab's own Makefile, or the root Makefile.
- Root `make build`/`test`/`benchmark`/`clean` run everything from one
  place; if `cargo` isn't on `PATH` (true of this repo's WSL install), they
  print `skip: <lab> -- cargo not found on PATH` per Rust lab and continue
  rather than aborting.
- `make sanitize` needs the WSL/Linux side (ASan+UBSan runtime).
- `make format`/`make lint` need the Windows/cargo side for the Rust checks;
  the C/C++ half of `make format` is report-only (see the Makefile's own
  comment on why) and needs `clang-format` on `PATH`, which the WSL install
  used here does not have.

There is no single command in this environment that exercises both sides at
once. `.github/workflows/ci.yml` is designed to (GitHub's `ubuntu-latest`
runner ships gcc/g++/make/python3 *and* cargo together) but has not been run
here -- this repository has no configured git remote in this environment, so
the workflow has never actually executed on GitHub Actions. Every step in it
was verified independently on one side or the other, as described above and
in `docs/engineering-audit.md`; that is not the same claim as "CI is green."

## Rebuilding this yourself

```bash
# Rust labs (from repo root, needs cargo on PATH):
cd algorithms/bloom-filter && cargo test && cargo fmt --check && cargo clippy
cd compilers/tiny-language && cargo test && cargo fmt --check && cargo clippy
cd databases/tiny-lsm && cargo test && cargo fmt --check && cargo clippy
cd distributed/raft-simulator && cargo test && cargo fmt --check && cargo clippy

# Everything else (from repo root, needs a POSIX make -- WSL/Linux/macOS):
make build
make test
make sanitize     # ASan+UBSan, needs Linux/glibc + gcc or clang
make format        # cargo fmt --check (skipped if no cargo) + clang-format report
make lint          # cargo clippy -D warnings (skipped if no cargo)
make check         # format + build + test
make benchmark
make clean
```

Every lab also builds/tests standalone from its own directory
(`make`/`make test` for C/C++, `cargo test` for Rust) without touching the
root Makefile.

## Determinism

Every experiment that uses randomness seeds it explicitly and prints the
seed:

- `algorithms/sat-solver/src/bench.cpp`: `std::mt19937 rng(42)`
- `architecture/branch-predictor/src/bench.cpp`: `pattern_random(n, p, 42)`
- `compilers/bytecode-vm/tests/test_vm.c`: `FUZZ_SEED 20260911u` (xorshift32,
  not `rand()`/`rand_r()` -- fully specified by the seed alone, no libc-
  dependent randomness)

`distributed/raft-simulator` uses no randomness at all: election timeouts
are staggered deterministically (`base + node_id * 10`) specifically so
cluster behavior is reproducible run to run (see its README).

No lab's test suite or benchmark relies on wall-clock-sensitive timing
*for correctness* -- only for benchmark *numbers*, which is a fundamentally
different property with its own caveats (see `docs/experiments.md` and
finding #3 in `docs/engineering-audit.md`).

## What "verified" means in this repository's docs

A claim of "passes" or "verified" in this repo's docs means a command was
actually run in one of the two environments above during this session and
its output inspected -- not that it was assumed, extrapolated, or run once
in the past and never rechecked. Where a claim couldn't be verified here
(GitHub Actions actually running; native-Windows sanitizer behavior), that
limitation is stated explicitly rather than implied away.
