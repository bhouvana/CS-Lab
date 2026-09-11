// Experiment: how does graph size affect BFS vs Dijkstra runtime?
// Both are O((V+E) log V)-ish here, but BFS does no heap work, so it
// should scale noticeably better as V grows.
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "algorithms.hpp"

namespace {
Graph random_graph(int n, int avg_degree, unsigned seed) {
    Graph g(n);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> node(0, n - 1);
    std::uniform_int_distribution<int> weight(1, 20);
    long long edges = (long long)n * avg_degree;
    for (long long i = 0; i < edges; i++) {
        int u = node(rng), v = node(rng);
        if (u != v) g.add_edge(u, v, weight(rng));
    }
    return g;
}

template <typename Fn>
double time_ms(Fn&& fn) {
    auto start = std::chrono::steady_clock::now();
    fn();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}
} // namespace

int main() {
    std::cout << "Benchmark: BFS vs Dijkstra runtime by graph size\n";
    std::cout << "(random graph, average out-degree 4, src=0, dst=n-1)\n\n";
    std::cout << std::left << std::setw(10) << "V" << std::setw(12) << "E"
              << std::setw(14) << "BFS (ms)" << std::setw(14) << "Dijkstra (ms)" << "\n";

    for (int n : {100, 1000, 10000, 50000}) {
        Graph g = random_graph(n, 4, /*seed=*/42);
        long long edges = 0;
        for (auto& e : g.adj) edges += (long long)e.size();

        double bfs_ms = time_ms([&] { bfs_shortest_path(g, 0, n - 1); });
        double dij_ms = time_ms([&] { dijkstra_shortest_path(g, 0, n - 1); });

        std::cout << std::left << std::setw(10) << n << std::setw(12) << edges
                   << std::setw(14) << std::fixed << std::setprecision(3) << bfs_ms
                   << std::setw(14) << dij_ms << "\n";
    }
    return 0;
}
