CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test test-asan benchmark clean

build: allocator_demo

allocator_demo: src/main.c src/allocator.c include/allocator.h
	$(CC) $(CFLAGS) src/main.c src/allocator.c -o allocator_demo

allocator_test: tests/test_allocator.c src/allocator.c include/allocator.h
	$(CC) $(CFLAGS) tests/test_allocator.c src/allocator.c -o allocator_test

# ASan/UBSan (CS-LAB.md §40) — a separate target, not the default `test`:
# this repo is built and tested on a MinGW/Windows toolchain that has no
# ASan/UBSan runtime libraries. Use this target on Linux/macOS/clang.
allocator_test_asan: tests/test_allocator.c src/allocator.c include/allocator.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g tests/test_allocator.c src/allocator.c -o allocator_test_asan

allocator_bench: src/bench.c src/allocator.c include/allocator.h
	$(CC) $(CFLAGS) src/bench.c src/allocator.c -o allocator_bench

test: allocator_test
	./allocator_test

test-asan: allocator_test_asan
	./allocator_test_asan

benchmark: allocator_bench
	./allocator_bench

clean:
	rm -f allocator_demo allocator_test allocator_test_asan allocator_bench
