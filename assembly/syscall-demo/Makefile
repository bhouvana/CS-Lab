# Linux x86-64 only -- raw `syscall` instruction, Linux syscall numbers
# and ABI (RAX/RDI/RSI/RDX/R10/R8/R9). Guarded, not just documented:
# executing a raw `syscall` with a Linux syscall number on a non-Linux
# kernel invokes THAT kernel's own (different) syscall table entry for
# the same number -- undefined behavior, not a clean failure. So
# build/test/benchmark/objdump no-op (not error, not attempt) off Linux.
UNAME_S := $(shell uname -s)
CC := gcc
CFLAGS := -Wall -Wextra -O2

.PHONY: build test benchmark strace-benchmark test-asan clean objdump

ifeq ($(UNAME_S),Linux)

build: demo hello_nolibc cat_raw

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

cat_raw: src/cat.c src/syscalls.S
	$(CC) $(CFLAGS) src/cat.c src/syscalls.S -o cat_raw

test: demo_test cat_raw
	./demo_test
	test "$$(printf 'syscall cat\n' | ./cat_raw)" = "syscall cat"

benchmark: demo_bench
	./demo_bench

strace-benchmark: demo_bench
	SYSCALL_BENCH_ITERATIONS=1000 strace -c -e trace=getpid,write ./demo_bench >/dev/null

test-asan: tests/test_syscalls.c src/syscalls.S
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g tests/test_syscalls.c src/syscalls.S -o demo_test_asan
	./demo_test_asan

# CS-LAB.md §26: objdump example -- the raw wrappers' actual machine code.
objdump: demo
	objdump -d demo -M att --disassemble=my_read --disassemble=my_write --disassemble=my_exit --disassemble=my_getpid

else

build test benchmark strace-benchmark test-asan objdump:
	@echo "skip: assembly/syscall-demo is Linux x86-64 only (see README) -- detected '$(UNAME_S)'"

endif

clean:
	rm -f demo demo_test demo_bench demo_test_asan cat_raw hello_nolibc syscall_bench.tmp tests/tmp_syscall_test.txt tests/tmp_syscall_test2.txt
