# Linux x86-64 only -- raw `syscall` instruction, Linux syscall numbers
# and ABI (RAX/RDI/RSI/RDX/R10/R8/R9). Guarded, not just documented:
# executing a raw `syscall` with a Linux syscall number on a non-Linux
# kernel invokes THAT kernel's own (different) syscall table entry for
# the same number -- undefined behavior, not a clean failure. So
# build/test/benchmark/objdump no-op (not error, not attempt) off Linux.
UNAME_S := $(shell uname -s)
CC := gcc
CFLAGS := -Wall -Wextra -O2

.PHONY: build test benchmark clean objdump

ifeq ($(UNAME_S),Linux)

build: demo hello_nolibc

demo: src/main.c src/syscalls.S
	$(CC) $(CFLAGS) src/main.c src/syscalls.S -o demo

# -nostdlib: no libc, no C runtime startup -- _start in hello_nolibc.S
# IS the entry point the kernel jumps to. This is the literal point of
# the lab, not an optimization.
hello_nolibc: src/hello_nolibc.S
	$(CC) -nostdlib -static -o hello_nolibc src/hello_nolibc.S

demo_test: tests/test_syscalls.c src/syscalls.S
	$(CC) $(CFLAGS) tests/test_syscalls.c src/syscalls.S -o demo_test

demo_bench: src/bench.c src/syscalls.S
	$(CC) $(CFLAGS) src/bench.c src/syscalls.S -o demo_bench

test: demo_test
	./demo_test

benchmark: demo_bench
	./demo_bench

# CS-LAB.md §26: objdump example -- my_write's actual machine code.
objdump: demo
	objdump -d demo -M att --disassemble=my_write --disassemble=my_exit

else

build test benchmark objdump:
	@echo "skip: assembly/syscall-demo is Linux x86-64 only (see README) -- detected '$(UNAME_S)'"

endif

clean:
	rm -f demo demo_test demo_bench hello_nolibc
