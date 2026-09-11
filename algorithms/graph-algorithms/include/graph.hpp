#pragma once
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Directed weighted graph, adjacency-list representation.
// Shared by every algorithm in this lab so each algorithm only needs
// to know how to walk `adj` — no algorithm owns its own graph type.
struct Graph {
    int n = 0;
    std::vector<std::vector<std::pair<int, int>>> adj; // adj[u] = {(v, weight), ...}

    explicit Graph(int n_) : n(n_), adj(n_) {}

    void add_edge(int u, int v, int w) {
        if (u < 0 || u >= n || v < 0 || v >= n)
            throw std::out_of_range("edge endpoint out of range");
        adj[u].push_back({v, w});
    }

    // File format:
    //   n m
    //   u v w      (repeated m times, weight required — use 1 for unweighted graphs)
    static Graph load(const std::string& path) {
        std::ifstream in(path);
        if (!in) throw std::runtime_error("cannot open " + path);
        int n = 0, m = 0;
        if (!(in >> n >> m)) throw std::runtime_error("bad header in " + path);
        Graph g(n);
        for (int i = 0; i < m; i++) {
            int u, v, w;
            if (!(in >> u >> v >> w)) throw std::runtime_error("bad edge line in " + path);
            g.add_edge(u, v, w);
        }
        return g;
    }
};
