// CLI:
//   graph bfs         <file> <src> <dst>
//   graph dijkstra    <file> <src> <dst>
//   graph bellmanford <file> <src> <dst>
//   graph dfs         <file> <src>
//   graph toposort    <file>
#include <iostream>
#include <stdexcept>

#include "algorithms.hpp"

namespace {
void print_path(const PathResult& r, const char* unit) {
    if (!r.reachable) {
        std::cout << "UNREACHABLE\n";
        return;
    }
    std::cout << "distance: " << r.distance << " " << unit << "\n";
    std::cout << "path: ";
    for (size_t i = 0; i < r.path.size(); i++) {
        if (i) std::cout << " -> ";
        std::cout << r.path[i];
    }
    std::cout << "\n";
}

int usage(const char* prog) {
    std::cerr << "usage:\n"
              << "  " << prog << " bfs         <file> <src> <dst>\n"
              << "  " << prog << " dijkstra    <file> <src> <dst>\n"
              << "  " << prog << " bellmanford <file> <src> <dst>\n"
              << "  " << prog << " dfs         <file> <src>\n"
              << "  " << prog << " toposort    <file>\n";
    return 2;
}
} // namespace

int run(int argc, char** argv) {
    if (argc < 3) return usage(argv[0]);
    std::string mode = argv[1];
    std::string file = argv[2];

    Graph g = Graph::load(file);

    if (mode == "bfs" || mode == "dijkstra" || mode == "bellmanford") {
        if (argc != 5) return usage(argv[0]);
        int src = std::stoi(argv[3]);
        int dst = std::stoi(argv[4]);
        PathResult r;
        if (mode == "bfs") {
            r = bfs_shortest_path(g, src, dst);
        } else if (mode == "dijkstra") {
            r = dijkstra_shortest_path(g, src, dst);
        } else {
            r = bellman_ford_shortest_path(g, src, dst); // may throw on a negative cycle
        }
        print_path(r, mode == "bfs" ? "hops" : "weight");
    } else if (mode == "dfs") {
        if (argc != 4) return usage(argv[0]);
        int src = std::stoi(argv[3]);
        auto order = dfs_preorder(g, src);
        std::cout << "preorder: ";
        for (size_t i = 0; i < order.size(); i++) {
            if (i) std::cout << " -> ";
            std::cout << order[i];
        }
        std::cout << "\n";
        auto cycle = dfs_find_cycle(g);
        std::cout << "has_cycle: " << (cycle ? "yes" : "no") << "\n";
        if (cycle) {
            std::cout << "cycle: ";
            for (size_t i = 0; i < cycle->size(); i++) {
                if (i) std::cout << " -> ";
                std::cout << (*cycle)[i];
            }
            std::cout << "\n";
        }
    } else if (mode == "toposort") {
        auto order = topological_sort(g);
        if (!order) {
            std::cout << "CYCLE DETECTED: no topological order exists\n";
            return 1;
        }
        std::cout << "order: ";
        for (size_t i = 0; i < order->size(); i++) {
            if (i) std::cout << " -> ";
            std::cout << (*order)[i];
        }
        std::cout << "\n";
    } else {
        return usage(argv[0]);
    }
    return 0;
}

int main(int argc, char** argv) {
    // A bad file (Graph::load), a negative-weight cycle
    // (bellman_ford_shortest_path), or a malformed numeric argument
    // (std::stoi) all throw -- without this, any of them was an
    // uncaught std::terminate()/SIGABRT crash, not a clean error.
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
