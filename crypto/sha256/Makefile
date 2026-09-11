CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test benchmark clean

build: sha256

sha256: src/main.c src/sha256.c include/sha256.h
	$(CC) $(CFLAGS) src/main.c src/sha256.c -o sha256

sha256_test: tests/test_sha256.c src/sha256.c include/sha256.h
	$(CC) $(CFLAGS) tests/test_sha256.c src/sha256.c -o sha256_test

sha256_bench: src/bench.c src/sha256.c include/sha256.h
	$(CC) $(CFLAGS) src/bench.c src/sha256.c -o sha256_bench

test: sha256_test
	./sha256_test

benchmark: sha256_bench
	./sha256_bench

clean:
	rm -f sha256 sha256_test sha256_bench
