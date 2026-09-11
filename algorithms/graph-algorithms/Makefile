CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test benchmark clean

build: graph

graph: src/main.cpp src/algorithms.cpp include/*.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/algorithms.cpp -o graph

graph_test: tests/test_algorithms.cpp src/algorithms.cpp include/*.hpp
	$(CXX) $(CXXFLAGS) tests/test_algorithms.cpp src/algorithms.cpp -o graph_test

graph_bench: src/bench.cpp src/algorithms.cpp include/*.hpp
	$(CXX) $(CXXFLAGS) src/bench.cpp src/algorithms.cpp -o graph_bench

test: graph_test
	./graph_test

benchmark: graph_bench
	./graph_bench

clean:
	rm -f graph graph_test graph_bench
