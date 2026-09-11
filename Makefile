# CS-Lab root Makefile.
# Drives every lab's own build system. A lab is "active" if it has a
# Makefile (C/C++) or Cargo.toml (Rust); labs not yet implemented are
# skipped automatically so this file needs no editing as labs are added.

LABS := $(shell find . -mindepth 3 -maxdepth 3 \( -name Makefile -o -name Cargo.toml \) -exec dirname {} \; | sort -u)
HAVE_CARGO := $(shell command -v cargo 2>/dev/null)
HAVE_CLANG_FORMAT := $(shell command -v clang-format 2>/dev/null)

.PHONY: build test benchmark clean list format lint sanitize check

build:
	@for d in $(LABS); do \
		if [ -f $$d/Cargo.toml ]; then \
			if [ -n "$(HAVE_CARGO)" ]; then echo "==> build $$d"; (cd $$d && cargo build --quiet) || exit 1; \
			else echo "skip: $$d -- cargo not found on PATH"; fi; \
		else echo "==> build $$d"; $(MAKE) -C $$d build || exit 1; fi; \
	done

test:
	@for d in $(LABS); do \
		if [ -f $$d/Cargo.toml ]; then \
			if [ -n "$(HAVE_CARGO)" ]; then echo "==> test $$d"; (cd $$d && cargo test --quiet) || exit 1; \
			else echo "skip: $$d -- cargo not found on PATH"; fi; \
		else echo "==> test $$d"; $(MAKE) -C $$d test || exit 1; fi; \
	done

benchmark:
	@for d in $(LABS); do \
		if [ -f $$d/Cargo.toml ]; then \
			if [ -n "$(HAVE_CARGO)" ]; then echo "==> benchmark $$d"; (cd $$d && cargo run --quiet --release 2>/dev/null || true); \
			else echo "skip: $$d -- cargo not found on PATH"; fi; \
		else echo "==> benchmark $$d"; $(MAKE) -C $$d benchmark 2>/dev/null || true; fi; \
	done

# Runs each lab's `test-asan` target (ASan+UBSan), where it has one.
# Needs a Linux/glibc toolchain with the sanitizer runtimes -- on this
# repo's Windows/MinGW dev box that means running under WSL/Linux, not
# natively. Labs without a test-asan target (see CS-LAB.md §12 for the
# priority list) are skipped, not failed.
sanitize:
	@for d in $(LABS); do \
		if [ -f $$d/Makefile ] && grep -q "^test-asan:" $$d/Makefile; then \
			echo "==> sanitize $$d"; $(MAKE) -C $$d test-asan || exit 1; \
		fi; \
	done

# Rust: `cargo fmt --check` per crate. C/C++: clang-format --dry-run is
# report-only here, not enforced -- this codebase predates .clang-format
# and reformatting ~30 already-reviewed, zero-warning source files to an
# arbitrary style would be exactly the unrelated churn CS-LAB.md §46
# warns against. .clang-format documents the style for new/touched code;
# it isn't retroactively applied. See docs/engineering-audit.md.
format:
	@for d in $(LABS); do \
		if [ -f $$d/Cargo.toml ]; then \
			if [ -n "$(HAVE_CARGO)" ]; then echo "==> fmt-check $$d"; (cd $$d && cargo fmt --check) || exit 1; \
			else echo "skip: $$d -- cargo not found on PATH"; fi; \
		fi; \
	done
	@if [ -n "$(HAVE_CLANG_FORMAT)" ]; then \
		echo "==> clang-format --dry-run (report only; not enforced, see comment above)"; \
		find . -regex '.*\.\(c\|h\|cpp\|hpp\)$$' -not -path '*/target/*' -print0 | xargs -0 clang-format --dry-run -style=file 2>&1 | grep -c "should be clang-formatted" | xargs -I{} echo "{} line(s) differ from .clang-format (informational)"; \
	else echo "skip: clang-format not found on PATH"; fi

lint:
	@for d in $(LABS); do \
		if [ -f $$d/Cargo.toml ]; then \
			if [ -n "$(HAVE_CARGO)" ]; then echo "==> clippy $$d"; (cd $$d && cargo clippy --quiet -- -D warnings) || exit 1; \
			else echo "skip: $$d -- cargo not found on PATH"; fi; \
		fi; \
	done
	@echo "note: C/C++ labs already build with -Wall -Wextra -Wpedantic and 0 warnings (verified by 'make build'); no separate C/C++ linter is wired in (no cppcheck/clang-tidy compilation database in this environment -- see docs/engineering-audit.md)."

# format + build + test: the fast local pre-commit gate. Sanitizers are
# a separate `make sanitize` because they need a Linux toolchain and
# take noticeably longer.
check: format build test

clean:
	@for d in $(LABS); do \
		if [ -f $$d/Cargo.toml ]; then \
			if [ -n "$(HAVE_CARGO)" ]; then (cd $$d && cargo clean --quiet); fi; \
		else $(MAKE) -C $$d clean 2>/dev/null || true; fi; \
	done

list:
	@echo "$(LABS)" | tr ' ' '\n'
