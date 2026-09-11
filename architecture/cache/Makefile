CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test benchmark clean

build: cache

cache: src/main.cpp src/cache.cpp src/parse_size.cpp include/cache.hpp include/parse_size.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/cache.cpp src/parse_size.cpp -o cache

cache_test: tests/test_cache.cpp src/cache.cpp src/parse_size.cpp include/cache.hpp include/parse_size.hpp
	$(CXX) $(CXXFLAGS) tests/test_cache.cpp src/cache.cpp src/parse_size.cpp -o cache_test

cache_bench: src/bench.cpp src/cache.cpp include/cache.hpp
	$(CXX) $(CXXFLAGS) src/bench.cpp src/cache.cpp -o cache_bench

test: cache_test
	./cache_test

benchmark: cache_bench
	./cache_bench

clean:
	rm -f cache cache_test cache_bench
