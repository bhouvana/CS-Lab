CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test test-asan benchmark clean

build: huffman

huffman: src/main.c src/huffman.c include/huffman.h
	$(CC) $(CFLAGS) src/main.c src/huffman.c -o huffman

huffman_test: tests/test_huffman.c src/huffman.c include/huffman.h
	$(CC) $(CFLAGS) tests/test_huffman.c src/huffman.c -o huffman_test

# ASan/UBSan (CS-LAB.md §40) as a separate target -- see os/allocator's
# README for why it isn't the default `test` on this repo's toolchain.
huffman_test_asan: tests/test_huffman.c src/huffman.c include/huffman.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g tests/test_huffman.c src/huffman.c -o huffman_test_asan

huffman_bench: src/bench.c src/huffman.c include/huffman.h
	$(CC) $(CFLAGS) src/bench.c src/huffman.c -o huffman_bench

test: huffman_test
	./huffman_test

test-asan: huffman_test_asan
	./huffman_test_asan

benchmark: huffman_bench
	./huffman_bench

clean:
	rm -f huffman huffman_test huffman_test_asan huffman_bench
