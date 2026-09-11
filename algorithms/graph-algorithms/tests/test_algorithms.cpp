// Plain assert-based tests — no test framework needed for a lab this size.
// Exits non-zero (via assert abort) on first failure.
#include <cassert>
#include <iostream>

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

int main() {
    test_bfs_normal();
    test_bfs_unreachable();
    test_bfs_src_equals_dst();
    test_dfs_preorder_visits_all_reachable();
    test_dfs_cycle_detection();
    test_dijkstra_picks_cheaper_path();
    test_topo_sort_dag_valid_order();
    test_topo_sort_cycle_returns_nullopt();
    std::cout << "all tests passed\n";
    return 0;
}
