CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test benchmark clean

build: branch

branch: src/main.cpp src/patterns.cpp include/predictors.hpp include/patterns.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/patterns.cpp -o branch

branch_test: tests/test_predictors.cpp src/patterns.cpp include/predictors.hpp include/patterns.hpp
	$(CXX) $(CXXFLAGS) tests/test_predictors.cpp src/patterns.cpp -o branch_test

branch_bench: src/bench.cpp src/patterns.cpp include/predictors.hpp include/patterns.hpp
	$(CXX) $(CXXFLAGS) src/bench.cpp src/patterns.cpp -o branch_bench

test: branch_test
	./branch_test

benchmark: branch_bench
	./branch_bench

clean:
	rm -f branch branch_test branch_bench
