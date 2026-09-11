CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test benchmark clean

build: lz77

lz77: src/main.cpp src/lz77.cpp include/lz77.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/lz77.cpp -o lz77

lz77_test: tests/test_lz77.cpp src/lz77.cpp include/lz77.hpp
	$(CXX) $(CXXFLAGS) tests/test_lz77.cpp src/lz77.cpp -o lz77_test

lz77_bench: src/bench.cpp src/lz77.cpp include/lz77.hpp
	$(CXX) $(CXXFLAGS) src/bench.cpp src/lz77.cpp -o lz77_bench

test: lz77_test
	./lz77_test

benchmark: lz77_bench
	./lz77_bench

clean:
	rm -f lz77 lz77_test lz77_bench
