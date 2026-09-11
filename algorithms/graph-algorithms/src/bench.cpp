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

    std::cout << "\nBenchmark: does graph *density* (not just size) widen the BFS/Dijkstra gap?\n";
    std::cout << "(fixed V=2000, src=0, dst=V-1, average out-degree varies)\n\n";
    std::cout << std::left << std::setw(10) << "avg_deg" << std::setw(12) << "E" << std::setw(14) << "BFS (ms)"
               << std::setw(14) << "Dijkstra (ms)" << std::setw(10) << "ratio" << "\n";
    for (int avg_degree : {2, 4, 8, 16, 32}) {
        const int n = 2000;
        Graph g = random_graph(n, avg_degree, /*seed=*/42);
        long long edges = 0;
        for (auto& e : g.adj) edges += (long long)e.size();

        double bfs_ms = time_ms([&] { bfs_shortest_path(g, 0, n - 1); });
        double dij_ms = time_ms([&] { dijkstra_shortest_path(g, 0, n - 1); });
        double ratio = bfs_ms > 0.0 ? dij_ms / bfs_ms : 0.0;

        std::cout << std::left << std::setw(10) << avg_degree << std::setw(12) << edges << std::setw(14)
                   << std::fixed << std::setprecision(3) << bfs_ms << std::setw(14) << dij_ms << std::setw(10)
                   << std::setprecision(1) << ratio << "\n";
    }

    std::cout << "\nBenchmark: Dijkstra vs Bellman-Ford runtime by graph size\n";
    std::cout << "(same random graphs as the first table -- both non-negative, so both give\n";
    std::cout << " the same answer; this measures O((V+E)logV) vs O(V*E) directly)\n\n";
    std::cout << std::left << std::setw(10) << "V" << std::setw(12) << "E" << std::setw(16) << "Dijkstra (ms)"
               << std::setw(18) << "Bellman-Ford (ms)" << "\n";
    for (int n : {100, 1000, 5000, 10000}) {
        Graph g = random_graph(n, 4, /*seed=*/42);
        long long edges = 0;
        for (auto& e : g.adj) edges += (long long)e.size();

        double dij_ms = time_ms([&] { dijkstra_shortest_path(g, 0, n - 1); });
        double bf_ms = time_ms([&] { bellman_ford_shortest_path(g, 0, n - 1); });

        std::cout << std::left << std::setw(10) << n << std::setw(12) << edges << std::setw(16) << std::fixed
                   << std::setprecision(3) << dij_ms << std::setw(18) << bf_ms << "\n";
    }
    return 0;
}
