# Debian/Ubuntu (incl. WSL) ship only `python3`; Windows/Git Bash and
# some distros still only have `python`. Prefer python3, fall back to
# python, so `make test`/`make benchmark` work unmodified on both.
PYTHON := $(shell command -v python3 2>/dev/null || command -v python 2>/dev/null || echo python3)

.PHONY: build test benchmark clean

build:
	@echo "nothing to build (pure Python)"

test:
	$(PYTHON) tests/test_pipeline.py

benchmark:
	$(PYTHON) src/bench.py

clean:
	@echo "nothing to clean (pure Python)"
