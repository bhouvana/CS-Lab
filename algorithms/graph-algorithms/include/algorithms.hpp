#pragma once
#include <optional>
#include <vector>

#include "graph.hpp"

// Result of a single-source, single-destination shortest-path query.
struct PathResult {
    bool reachable = false;
    long long distance = -1;
    std::vector<int> path; // src..dst inclusive, empty if unreachable
};

// BFS: shortest path by edge *count*, ignores weights. O(V + E).
PathResult bfs_shortest_path(const Graph& g, int src, int dst);

// DFS preorder traversal starting at src (iterative, explicit stack).
std::vector<int> dfs_preorder(const Graph& g, int src);

// True if the graph (considered over all vertices, not just one
// component) contains a directed cycle.
bool dfs_has_cycle(const Graph& g);

// Dijkstra: shortest path by total edge weight. Requires non-negative
// weights (asserted). O((V + E) log V) via a binary heap.
PathResult dijkstra_shortest_path(const Graph& g, int src, int dst);

// Kahn's algorithm. Returns nullopt if the graph has a cycle (no valid
// topological order exists).
std::optional<std::vector<int>> topological_sort(const Graph& g);
