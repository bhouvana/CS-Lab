# Linux/POSIX x86-64 only. Requires gdb for `make build`'s walk output
# (gracefully skipped by tests if gdb isn't installed -- see README).
# Guarded to Linux specifically (not just "any Unix"): the frame-size
# experiment's whole point is the x86-64 System V red zone, which
# doesn't exist in the Windows x64 ABI, and relies on ELF/objdump
# output this lab's tooling assumes.
UNAME_S := $(shell uname -s)
CC := gcc
CFLAGS := -O0 -g -fno-omit-frame-pointer -Wall -Wextra
PYTHON := python3

.PHONY: build test benchmark experiments clean objdump

ifeq ($(UNAME_S),Linux)

build: nested
	@echo
	@$(PYTHON) scripts/walk_stack.py ./nested || echo "(gdb not available -- see README)"

nested: src/nested.c
	$(CC) $(CFLAGS) src/nested.c -o nested

test: nested
	$(PYTHON) tests/test_walk_stack.py
	$(PYTHON) tests/test_further_experiments.py

benchmark: nested
	$(PYTHON) scripts/frame_size_experiment.py
	$(PYTHON) scripts/further_experiments.py

experiments: nested
	$(PYTHON) scripts/further_experiments.py

# CS-LAB.md §26: objdump example -- the real prologue/epilogue GCC
# generates for each function (push %rbp; mov %rsp,%rbp; ...; leave; ret).
objdump: nested
	objdump -d nested -M att --disassemble=bar --disassemble=foo --disassemble=main

else

build test benchmark objdump:
	@echo "skip: assembly/stack-frames is Linux x86-64 only (see README) -- detected '$(UNAME_S)'"

endif

clean:
	rm -f nested
