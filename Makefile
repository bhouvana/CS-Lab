# CS-Lab root Makefile.
# Drives every lab's own build system. A lab is "active" if it has a
# Makefile (C/C++) or Cargo.toml (Rust); labs not yet implemented are
# skipped automatically so this file needs no editing as labs are added.

LABS := $(shell find . -mindepth 3 -maxdepth 3 \( -name Makefile -o -name Cargo.toml \) -exec dirname {} \; | sort -u)

.PHONY: build test benchmark clean list

build:
	@for d in $(LABS); do \
		echo "==> build $$d"; \
		if [ -f $$d/Cargo.toml ]; then (cd $$d && cargo build --quiet) || exit 1; \
		else $(MAKE) -C $$d build || exit 1; fi; \
	done

test:
	@for d in $(LABS); do \
		echo "==> test $$d"; \
		if [ -f $$d/Cargo.toml ]; then (cd $$d && cargo test --quiet) || exit 1; \
		else $(MAKE) -C $$d test || exit 1; fi; \
	done

benchmark:
	@for d in $(LABS); do \
		echo "==> benchmark $$d"; \
		if [ -f $$d/Cargo.toml ]; then (cd $$d && cargo run --quiet --release 2>/dev/null || true); \
		else $(MAKE) -C $$d benchmark 2>/dev/null || true; fi; \
	done

clean:
	@for d in $(LABS); do \
		if [ -f $$d/Cargo.toml ]; then (cd $$d && cargo clean --quiet); \
		else $(MAKE) -C $$d clean 2>/dev/null || true; fi; \
	done

list:
	@echo "$(LABS)" | tr ' ' '\n'
