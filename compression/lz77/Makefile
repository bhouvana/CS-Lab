CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test test-asan benchmark clean

build: lz77

lz77: src/main.cpp src/lz77.cpp include/lz77.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/lz77.cpp -o lz77

lz77_test: tests/test_lz77.cpp src/lz77.cpp include/lz77.hpp
	$(CXX) $(CXXFLAGS) tests/test_lz77.cpp src/lz77.cpp -o lz77_test

# ASan/UBSan (CS-LAB.md §40) as a separate target -- see os/allocator's
# README for why it isn't the default `test` on this repo's toolchain.
lz77_test_asan: tests/test_lz77.cpp src/lz77.cpp include/lz77.hpp
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -g tests/test_lz77.cpp src/lz77.cpp -o lz77_test_asan

lz77_bench: src/bench.cpp src/lz77.cpp include/lz77.hpp
	$(CXX) $(CXXFLAGS) src/bench.cpp src/lz77.cpp -o lz77_bench

test: lz77_test
	./lz77_test

test-asan: lz77_test_asan
	./lz77_test_asan

benchmark: lz77_bench
	./lz77_bench

clean:
	rm -f lz77 lz77_test lz77_test_asan lz77_bench
