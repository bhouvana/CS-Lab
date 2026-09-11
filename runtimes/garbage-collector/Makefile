CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test test-asan benchmark experiments clean

build: gc_demo

gc_demo: src/main.c src/gc.c include/gc.h
	$(CC) $(CFLAGS) src/main.c src/gc.c -o gc_demo

gc_test: tests/test_gc.c src/gc.c include/gc.h
	$(CC) $(CFLAGS) tests/test_gc.c src/gc.c -o gc_test

# ASan/UBSan (CS-LAB.md §40) as a separate target -- see os/allocator's
# README for why it isn't the default `test` on this repo's toolchain.
gc_test_asan: tests/test_gc.c src/gc.c include/gc.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g tests/test_gc.c src/gc.c -o gc_test_asan

gc_bench: src/bench.c src/gc.c include/gc.h
	$(CC) $(CFLAGS) src/bench.c src/gc.c -o gc_bench

test: gc_test
	./gc_test

test-asan: gc_test_asan
	./gc_test_asan

benchmark: gc_bench
	./gc_bench

gc_experiments: src/further_experiments.c
	$(CC) $(CFLAGS) src/further_experiments.c -o gc_experiments

experiments: gc_experiments
	./gc_experiments

clean:
	rm -f gc_demo gc_test gc_test_asan gc_bench gc_experiments
