# Graph Algorithms

## What is this?

A small C++ CLI implementing four classic graph algorithms — BFS, DFS,
Dijkstra, and topological sort — over one shared graph representation.

```bash
./graph bfs      <file> <src> <dst>
./graph dijkstra <file> <src> <dst>
./graph dfs      <file> <src>
./graph toposort <file>
```

## Why does it matter?

Almost every "shortest path", "is this reachable", "in what order should
these run" problem reduces to one of these four algorithms. Seeing them
share one `Graph` struct also makes clear that the *algorithm* is what
differs, not the data structure.

## Concept

```text
INPUT (edge list)
      |
      v
Graph (adjacency list)
      |
      v
BFS / DFS / Dijkstra / Topological Sort
      |
      v
OUTPUT (path + distance, or order, or cycle report)
```

- **BFS** — shortest path by edge *count* in an unweighted (or
  weight-ignoring) graph. Frontier-by-frontier expansion guarantees the
  first time you reach a node is via a shortest path.
- **Dijkstra** — shortest path by total edge *weight*, non-negative only.
  Always expands the frontier node with the smallest known distance
  (a min-heap), so once a node is popped its distance is final.
- **DFS** — depth-first traversal; also used here for cycle detection via
  three-coloring (white/gray/black) — a back edge to a gray node means a
  cycle.
- **Topological sort** — Kahn's algorithm: repeatedly remove nodes with
  in-degree 0. If nodes remain stuck above in-degree 0, the graph has a
  cycle and no valid order exists.

## How it works

All four algorithms operate on `include/graph.hpp`'s `Graph`: an adjacency
list `vector<vector<pair<int,int>>>` (neighbor, weight). Loading a graph
reads a simple text format:

```text
n m
u v w      (repeated m times)
```

## Implementation

- `include/graph.hpp` — shared graph type + file loader.
- `include/algorithms.hpp` / `src/algorithms.cpp` — the four algorithms.
- `src/main.cpp` — CLI dispatch.
- `src/bench.cpp` — the runtime experiment (below).
- `tests/test_algorithms.cpp` — assert-based tests, no framework.

## Example

```text
$ ./graph bfs examples/dag.txt 0 4
distance: 3 hops
path: 0 -> 1 -> 3 -> 4

$ ./graph dijkstra examples/weighted.txt 0 4
distance: 4 weight
path: 0 -> 1 -> 2 -> 4

$ ./graph dfs examples/dag.txt 0
preorder: 0 -> 1 -> 3 -> 4 -> 2
has_cycle: no

$ ./graph toposort examples/dag.txt
order: 0 -> 1 -> 2 -> 3 -> 4

$ ./graph toposort examples/cycle.txt
CYCLE DETECTED: no topological order exists
```

`examples/weighted.txt` deliberately includes a direct 0->4 edge of weight
10 alongside the cheaper 0-1-2-4 path (weight 4), so Dijkstra's answer is
verifiably not just "the first path found".

## Experiments

**Does Dijkstra's heap overhead show up as graphs grow, relative to BFS?**
`src/bench.cpp` builds random graphs (average out-degree 4, fixed seed) of
increasing size and times both algorithms end-to-end (graph construction
excluded).

Real output from `make benchmark`:

```text
V         E           BFS (ms)      Dijkstra (ms)
100       393         0.011         0.022
1000      3997        0.030         0.165
10000     39996       0.345         1.962
50000     199996      1.044         15.649
```

## Results

From 100 to 50,000 vertices (500x), BFS's runtime grew about 95x while
Dijkstra's grew about 710x. BFS does O(1) work per edge (queue push/pop);
Dijkstra does O(log V) work per edge (heap push, plus stale-entry checks),
so its relative cost grows with graph size — exactly what the O(V+E) vs
O((V+E) log V) complexity bounds predict.

## What I learned

The complexity-theory gap between BFS and Dijkstra is easy to state and
easy to forget matters in practice; here it's directly visible as a
widening runtime gap on the same hardware, same graphs, same code style.

## Limitations

- No negative-weight support (Dijkstra asserts non-negative weights;
  Bellman-Ford is out of scope for this lab).
- Graphs are directed only; undirected graphs must be encoded as two
  opposite directed edges.
- No weighted topological sort / longest-path-in-DAG variant.

## Further experiments

- Compare Dijkstra vs. Bellman-Ford on a graph with negative weights (but
  no negative cycle).
- Measure how graph density (not just size) affects the BFS/Dijkstra gap.
- Extend DFS cycle detection to report the actual cycle, not just yes/no.
