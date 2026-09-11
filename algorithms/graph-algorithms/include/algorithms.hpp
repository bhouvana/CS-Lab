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

// Bellman-Ford: shortest path by total edge weight, allows negative
// weights. O(V * E) (relaxes every edge V-1 times). Throws
// std::runtime_error if a further relaxation is still possible after
// V-1 passes -- a negative-weight cycle reachable from src, for which
// no shortest path is well-defined at all.
PathResult bellman_ford_shortest_path(const Graph& g, int src, int dst);

// Same three-color DFS cycle check as dfs_has_cycle, but returns the
// actual cycle (as v0 -> v1 -> ... -> v0, closing the loop) instead of
// just yes/no. nullopt if the graph is acyclic.
std::optional<std::vector<int>> dfs_find_cycle(const Graph& g);

// Kahn's algorithm. Returns nullopt if the graph has a cycle (no valid
// topological order exists).
std::optional<std::vector<int>> topological_sort(const Graph& g);
