CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test test-asan benchmark clean

build: vm

vm: src/main.c src/vm.c src/assembler.c include/vm.h
	$(CC) $(CFLAGS) src/main.c src/vm.c src/assembler.c -o vm

vm_test: tests/test_vm.c src/vm.c src/assembler.c include/vm.h
	$(CC) $(CFLAGS) tests/test_vm.c src/vm.c src/assembler.c -o vm_test

# ASan/UBSan (CS-LAB.md §40) as a separate target -- see os/allocator's
# README for why it isn't the default `test` on this repo's toolchain.
vm_test_asan: tests/test_vm.c src/vm.c src/assembler.c include/vm.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g tests/test_vm.c src/vm.c src/assembler.c -o vm_test_asan

vm_bench: src/bench.c src/vm.c src/assembler.c include/vm.h
	$(CC) $(CFLAGS) src/bench.c src/vm.c src/assembler.c -o vm_bench

test: vm_test
	./vm_test

test-asan: vm_test_asan
	./vm_test_asan

benchmark: vm_bench
	./vm_bench

clean:
	rm -f vm vm_test vm_test_asan vm_bench
