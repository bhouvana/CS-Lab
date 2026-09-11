CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -O0

.PHONY: build test test-asan benchmark benchmark-opt experiments clean

# -O0 deliberately: an optimizing compiler is free to notice
# constant_time_compare's loop has no early exit and vectorize/unroll
# it, which is good for real use but would make the two functions'
# *measured* timing gap in this lab less clean to observe. See README.

build: compare_demo

compare_demo: src/main.c src/compare.c include/compare.h
	$(CC) $(CFLAGS) src/main.c src/compare.c -o compare_demo

compare_test: tests/test_compare.c src/compare.c include/compare.h
	$(CC) $(CFLAGS) tests/test_compare.c src/compare.c -o compare_test

compare_test_asan: tests/test_compare.c src/compare.c include/compare.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g tests/test_compare.c src/compare.c -o compare_test_asan

compare_bench: src/bench.c src/compare.c include/compare.h
	$(CC) $(CFLAGS) src/bench.c src/compare.c -o compare_bench

compare_bench_opt: src/bench.c src/compare.c include/compare.h
	$(CC) $(CFLAGS) -O2 src/bench.c src/compare.c -o compare_bench_opt

followup_experiments: src/followup.c src/compare.c include/compare.h ../../crypto/sha256/src/sha256.c ../../crypto/sha256/include/sha256.h
	$(CC) $(CFLAGS) -I../../crypto/sha256/include src/followup.c src/compare.c ../../crypto/sha256/src/sha256.c -o followup_experiments $(if $(filter Windows_NT,$(OS)),-lws2_32,)

test: compare_test
	./compare_test

test-asan: compare_test_asan
	./compare_test_asan

benchmark: compare_bench
	./compare_bench

benchmark-opt: compare_bench_opt
	./compare_bench_opt

experiments: followup_experiments
	./followup_experiments

clean:
	rm -f compare_demo compare_test compare_test_asan compare_bench
