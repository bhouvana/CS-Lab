# fork()/execvp()/waitpid() are POSIX; this lab is Linux/POSIX only, same
# as networking/tcp-chat.
UNAME_S := $(shell uname -s)
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test test-asan benchmark clean

ifeq ($(UNAME_S),Linux)

build: shell

shell: src/main.c src/shell.c include/shell.h
	$(CC) $(CFLAGS) src/main.c src/shell.c -o shell

shell_test: tests/test_shell.c src/shell.c include/shell.h
	$(CC) $(CFLAGS) tests/test_shell.c src/shell.c -o shell_test

# ASan/UBSan (CS-LAB.md §40) as a separate target -- see os/allocator's
# README for why it isn't the default `test` on this repo's toolchain.
shell_test_asan: tests/test_shell.c src/shell.c include/shell.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g tests/test_shell.c src/shell.c -o shell_test_asan

shell_bench: src/bench.c
	$(CC) $(CFLAGS) src/bench.c -o shell_bench

test: shell shell_test
	./shell_test

test-asan: shell shell_test_asan
	./shell_test_asan

benchmark: shell shell_bench
	./shell_bench

else

build test test-asan benchmark:
	@echo "skip: os/shell is Linux/POSIX only (see README) -- detected '$(UNAME_S)'"

endif

clean:
	rm -f shell shell_test shell_test_asan shell_bench
