// Plain assert-based tests — no test framework needed for a lab this size.
// Exits non-zero (via assert abort) on first failure.
#include <cassert>
#include <iostream>
#include <stdexcept>

#include "algorithms.hpp"

static Graph make_dag() {
    // 0 -> 1 -> 3
    // 0 -> 2 -> 3
    // 3 -> 4
    Graph g(5);
    g.add_edge(0, 1, 1);
    g.add_edge(0, 2, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(2, 3, 1);
    g.add_edge(3, 4, 1);
    return g;
}

static Graph make_cycle() {
    // 0 -> 1 -> 2 -> 0
    Graph g(3);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(2, 0, 1);
    return g;
}

static Graph make_weighted() {
    // classic small weighted graph, shortest 0->4 is 0-1-2-4 = 1+2+1 = 4
    // (direct 0->4 costs 10)
    Graph g(5);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 2);
    g.add_edge(2, 4, 1);
    g.add_edge(0, 4, 10);
    g.add_edge(0, 3, 7);
    g.add_edge(3, 4, 1);
    return g;
}

static void test_bfs_normal() {
    Graph g = make_dag();
    PathResult r = bfs_shortest_path(g, 0, 4);
    assert(r.reachable);
    assert(r.distance == 3); // 0->1->3->4 or 0->2->3->4, both length 3
    assert(r.path.front() == 0 && r.path.back() == 4);
}

static void test_bfs_unreachable() {
    Graph g(3); // no edges at all
    PathResult r = bfs_shortest_path(g, 0, 2);
    assert(!r.reachable);
    assert(r.path.empty());
}

static void test_bfs_src_equals_dst() {
    Graph g = make_dag();
    PathResult r = bfs_shortest_path(g, 2, 2);
    assert(r.reachable);
    assert(r.distance == 0);
    assert(r.path.size() == 1 && r.path[0] == 2);
}

static void test_dfs_preorder_visits_all_reachable() {
    Graph g = make_dag();
    auto order = dfs_preorder(g, 0);
    assert(order.size() == 5); // all 5 nodes reachable from 0
    assert(order[0] == 0);
}

static void test_dfs_cycle_detection() {
    assert(dfs_has_cycle(make_cycle()) == true);
    assert(dfs_has_cycle(make_dag()) == false);
}

static void test_dijkstra_picks_cheaper_path() {
    Graph g = make_weighted();
    PathResult r = dijkstra_shortest_path(g, 0, 4);
    assert(r.reachable);
    assert(r.distance == 4);
    std::vector<int> expected{0, 1, 2, 4};
    assert(r.path == expected);
}

static void test_topo_sort_dag_valid_order() {
    Graph g = make_dag();
    auto order = topological_sort(g);
    assert(order.has_value());
    // Every edge u->v must have u appear before v in the order.
    std::vector<int> pos(g.n);
    for (size_t i = 0; i < order->size(); i++) pos[(*order)[i]] = (int)i;
    assert(pos[0] < pos[1]);
    assert(pos[0] < pos[2]);
    assert(pos[1] < pos[3]);
    assert(pos[2] < pos[3]);
    assert(pos[3] < pos[4]);
}

static void test_topo_sort_cycle_returns_nullopt() {
    auto order = topological_sort(make_cycle());
    assert(!order.has_value());
}

static Graph make_negative_no_cycle() {
    // Same graph as examples/negative.txt: a negative edge (2->1, -2)
    // makes the direct-looking 0->1->3 path (4+5=9) worse than routing
    // through it: 0->2->1->3 = 1 + -2 + 5 = 4. No negative *cycle* --
    // nothing ever points back into {0,1,2}'s negative edge.
    Graph g(5);
    g.add_edge(0, 1, 4);
    g.add_edge(0, 2, 1);
    g.add_edge(2, 1, -2);
    g.add_edge(1, 3, 5);
    g.add_edge(2, 3, 8);
    g.add_edge(3, 4, 3);
    return g;
}

static Graph make_negative_cycle() {
    // 0 -> 1 (1), 1 -> 2 (-3), 2 -> 1 (1): the 1<->2 loop has total
    // weight -3 + 1 = -2, so "shortest path" through it is unbounded.
    Graph g(3);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, -3);
    g.add_edge(2, 1, 1);
    return g;
}

static void test_bellman_ford_matches_dijkstra_on_nonnegative_normal_case() {
    Graph g = make_weighted();
    PathResult bf = bellman_ford_shortest_path(g, 0, 4);
    PathResult dij = dijkstra_shortest_path(g, 0, 4);
    assert(bf.reachable == dij.reachable);
    assert(bf.distance == dij.distance);
    assert(bf.path == dij.path);
}

static void test_bellman_ford_handles_negative_edge_normal_case() {
    // The whole point: Dijkstra would refuse this graph (assert on the
    // negative edge); Bellman-Ford finds the genuinely shortest path,
    // which routes *through* the negative edge rather than around it.
    Graph g = make_negative_no_cycle();
    PathResult r = bellman_ford_shortest_path(g, 0, 3);
    assert(r.reachable);
    assert(r.distance == 4); // 0->2->1->3 = 1 + (-2) + 5
    std::vector<int> expected{0, 2, 1, 3};
    assert(r.path == expected);
}

static void test_bellman_ford_detects_negative_cycle_invalid_case() {
    bool threw = false;
    try {
        bellman_ford_shortest_path(make_negative_cycle(), 0, 2);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

static void test_dfs_find_cycle_returns_actual_cycle_normal_case() {
    auto cycle = dfs_find_cycle(make_cycle()); // 0 -> 1 -> 2 -> 0
    assert(cycle.has_value());
    assert(cycle->size() >= 2);
    assert(cycle->front() == cycle->back()); // closes the loop
    // Every consecutive pair must be a real edge in the graph.
    Graph g = make_cycle();
    for (size_t i = 0; i + 1 < cycle->size(); i++) {
        int u = (*cycle)[i], v = (*cycle)[i + 1];
        bool found = false;
        for (auto [nb, w] : g.adj[u]) {
            (void)w;
            if (nb == v) found = true;
        }
        assert(found);
    }
}

static void test_dfs_find_cycle_acyclic_edge_case() {
    assert(!dfs_find_cycle(make_dag()).has_value());
}

int main() {
    test_bfs_normal();
    test_bfs_unreachable();
    test_bfs_src_equals_dst();
    test_dfs_preorder_visits_all_reachable();
    test_dfs_cycle_detection();
    test_dijkstra_picks_cheaper_path();
    test_topo_sort_dag_valid_order();
    test_topo_sort_cycle_returns_nullopt();
    test_bellman_ford_matches_dijkstra_on_nonnegative_normal_case();
    test_bellman_ford_handles_negative_edge_normal_case();
    test_bellman_ford_detects_negative_cycle_invalid_case();
    test_dfs_find_cycle_returns_actual_cycle_normal_case();
    test_dfs_find_cycle_acyclic_edge_case();
    std::cout << "all tests passed\n";
    return 0;
}
