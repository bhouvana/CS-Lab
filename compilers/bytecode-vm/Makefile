CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test benchmark clean

build: vm

vm: src/main.c src/vm.c src/assembler.c include/vm.h
	$(CC) $(CFLAGS) src/main.c src/vm.c src/assembler.c -o vm

vm_test: tests/test_vm.c src/vm.c src/assembler.c include/vm.h
	$(CC) $(CFLAGS) tests/test_vm.c src/vm.c src/assembler.c -o vm_test

vm_bench: src/bench.c src/vm.c src/assembler.c include/vm.h
	$(CC) $(CFLAGS) src/bench.c src/vm.c src/assembler.c -o vm_bench

test: vm_test
	./vm_test

benchmark: vm_bench
	./vm_bench

clean:
	rm -f vm vm_test vm_bench
