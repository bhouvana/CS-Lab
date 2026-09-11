# Linux/POSIX x86-64 only (System V calling convention, GAS AT&T
# syntax). This is not a portability nicety: the hand-written asm here
# uses the System V register-passing convention (RDI/RSI/RDX/...),
# which differs from Windows x64 (RCX/RDX/R8/R9) -- building this with
# a Windows toolchain would silently pass arguments in the wrong
# registers and produce wrong results, not just fail to build. So
# build/test/benchmark/objdump no-op (not error) off Linux.
UNAME_S := $(shell uname -s)
CC := gcc
CFLAGS := -Wall -Wextra -O2

.PHONY: build test benchmark clean objdump

ifeq ($(UNAME_S),Linux)

build: demo

demo: src/main.c src/math.S
	$(CC) $(CFLAGS) src/main.c src/math.S -o demo

demo_test: tests/test_calling_convention.c src/math.S
	$(CC) $(CFLAGS) tests/test_calling_convention.c src/math.S -o demo_test

demo_bench: src/bench.c src/math.S
	$(CC) $(CFLAGS) src/bench.c src/math.S -o demo_bench

test: demo_test
	./demo_test

benchmark: demo_bench
	./demo_bench

# CS-LAB.md §26: always include an objdump/gcc -S example. Compares
# our hand-written add2 against what the compiler generates for the
# same operation in C.
objdump: demo
	@echo "=== add2, hand-written (src/math.S), disassembled from the binary ==="
	@objdump -d demo --disassemble=add2 -M att
	@echo
	@echo "=== add2_c, compiler-generated from examples/add_equivalent.c (-O0 -S) ==="
	@gcc -O0 -S examples/add_equivalent.c -o -

else

build test benchmark objdump:
	@echo "skip: assembly/calling-convention is Linux x86-64 only (see README) -- detected '$(UNAME_S)'"

endif

clean:
	rm -f demo demo_test demo_bench
