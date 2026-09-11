#include "algorithms.hpp"

#include <algorithm>
#include <cassert>
#include <queue>

namespace {
// Rebuild src->dst path from a parent array. -2 = unvisited, -1 = is src.
std::vector<int> reconstruct(const std::vector<int>& parent, int src, int dst) {
    std::vector<int> path;
    for (int at = dst; at != -1; at = parent[at]) {
        path.push_back(at);
        if (at == src) break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}
} // namespace

PathResult bfs_shortest_path(const Graph& g, int src, int dst) {
    std::vector<int> dist(g.n, -1), parent(g.n, -2);
    std::queue<int> q;
    dist[src] = 0;
    parent[src] = -1;
    q.push(src);
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        if (u == dst) break;
        for (auto [v, w] : g.adj[u]) {
            (void)w; // BFS ignores weight — every edge costs 1 hop
            if (dist[v] == -1) {
                dist[v] = dist[u] + 1;
                parent[v] = u;
                q.push(v);
            }
        }
    }
    PathResult r;
    r.reachable = dist[dst] != -1;
    if (r.reachable) {
        r.distance = dist[dst];
        r.path = reconstruct(parent, src, dst);
    }
    return r;
}

std::vector<int> dfs_preorder(const Graph& g, int src) {
    std::vector<bool> visited(g.n, false);
    std::vector<int> order;
    std::vector<int> stack{src};
    while (!stack.empty()) {
        int u = stack.back();
        stack.pop_back();
        if (visited[u]) continue;
        visited[u] = true;
        order.push_back(u);
        // Push neighbors in reverse so the smallest index is visited first
        // (matches the intuitive recursive-DFS order); purely cosmetic.
        for (auto it = g.adj[u].rbegin(); it != g.adj[u].rend(); ++it)
            if (!visited[it->first]) stack.push_back(it->first);
    }
    return order;
}

bool dfs_has_cycle(const Graph& g) {
    // 0 = white (unvisited), 1 = gray (on current DFS stack), 2 = black (done).
    // A back edge to a gray vertex means a cycle.
    std::vector<int> color(g.n, 0);

    // Iterative DFS with explicit call-frame simulation so this works on
    // graphs too deep for the real call stack, and so we can tell "enter
    // node" from "finished node" (needed to flip gray -> black).
    struct Frame {
        int node;
        size_t edge_index;
    };

    for (int start = 0; start < g.n; start++) {
        if (color[start] != 0) continue;
        std::vector<Frame> stack{{start, 0}};
        color[start] = 1;
        while (!stack.empty()) {
            Frame& f = stack.back();
            if (f.edge_index < g.adj[f.node].size()) {
                int v = g.adj[f.node][f.edge_index++].first;
                if (color[v] == 1) return true; // back edge -> cycle
                if (color[v] == 0) {
                    color[v] = 1;
                    stack.push_back({v, 0});
                }
            } else {
                color[f.node] = 2;
                stack.pop_back();
            }
        }
    }
    return false;
}

PathResult dijkstra_shortest_path(const Graph& g, int src, int dst) {
    for (const auto& edges : g.adj)
        for (auto [v, w] : edges) {
            (void)v;
            assert(w >= 0 && "dijkstra requires non-negative edge weights");
        }

    const long long INF = 1LL << 60;
    std::vector<long long> dist(g.n, INF);
    std::vector<int> parent(g.n, -2);
    using QItem = std::pair<long long, int>; // (distance, vertex)
    std::priority_queue<QItem, std::vector<QItem>, std::greater<>> pq;

    dist[src] = 0;
    parent[src] = -1;
    pq.push({0, src});
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u]) continue; // stale entry
        for (auto [v, w] : g.adj[u]) {
            long long nd = d + w;
            if (nd < dist[v]) {
                dist[v] = nd;
                parent[v] = u;
                pq.push({nd, v});
            }
        }
    }

    PathResult r;
    r.reachable = dist[dst] != INF;
    if (r.reachable) {
        r.distance = dist[dst];
        r.path = reconstruct(parent, src, dst);
    }
    return r;
}

std::optional<std::vector<int>> topological_sort(const Graph& g) {
    std::vector<int> indegree(g.n, 0);
    for (const auto& edges : g.adj)
        for (auto [v, w] : edges) {
            (void)w;
            indegree[v]++;
        }

    std::queue<int> q;
    for (int i = 0; i < g.n; i++)
        if (indegree[i] == 0) q.push(i);

    std::vector<int> order;
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        order.push_back(u);
        for (auto [v, w] : g.adj[u]) {
            (void)w;
            if (--indegree[v] == 0) q.push(v);
        }
    }

    if ((int)order.size() != g.n) return std::nullopt; // cycle: some node never hit indegree 0
    return order;
}
