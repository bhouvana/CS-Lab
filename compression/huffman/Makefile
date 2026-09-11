CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test benchmark clean

build: huffman

huffman: src/main.c src/huffman.c include/huffman.h
	$(CC) $(CFLAGS) src/main.c src/huffman.c -o huffman

huffman_test: tests/test_huffman.c src/huffman.c include/huffman.h
	$(CC) $(CFLAGS) tests/test_huffman.c src/huffman.c -o huffman_test

huffman_bench: src/bench.c src/huffman.c include/huffman.h
	$(CC) $(CFLAGS) src/bench.c src/huffman.c -o huffman_bench

test: huffman_test
	./huffman_test

benchmark: huffman_bench
	./huffman_bench

clean:
	rm -f huffman huffman_test huffman_bench
