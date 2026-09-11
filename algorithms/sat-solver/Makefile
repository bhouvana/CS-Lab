CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -O2

.PHONY: build test benchmark clean

build: sat

sat: src/main.cpp src/sat.cpp src/dimacs.cpp include/sat.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/sat.cpp src/dimacs.cpp -o sat

sat_test: tests/test_sat.cpp src/sat.cpp src/dimacs.cpp include/sat.hpp
	$(CXX) $(CXXFLAGS) tests/test_sat.cpp src/sat.cpp src/dimacs.cpp -o sat_test

sat_bench: src/bench.cpp src/sat.cpp include/sat.hpp
	$(CXX) $(CXXFLAGS) src/bench.cpp src/sat.cpp -o sat_bench

test: sat_test
	./sat_test

benchmark: sat_bench
	./sat_bench

clean:
	rm -f sat sat_test sat_bench
