# Graph Algorithms

## What is this?

A small C++ CLI implementing five classic graph algorithms — BFS, DFS,
Dijkstra, Bellman-Ford, and topological sort — over one shared graph
representation.

```bash
./graph bfs         <file> <src> <dst>
./graph dijkstra    <file> <src> <dst>
./graph bellmanford <file> <src> <dst>
./graph dfs         <file> <src>
./graph toposort    <file>
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
- **Bellman-Ford** — shortest path by total edge weight, same as
  Dijkstra, but allows negative weights: relax every edge `V-1` times.
  A `V`-th pass that can still relax an edge means a negative-weight
  cycle is reachable from `src`, for which "shortest path" has no
  answer — this raises rather than returning a wrong number.
- **DFS** — depth-first traversal; also used here for cycle detection via
  three-coloring (white/gray/black) — a back edge to a gray node means a
  cycle, and walking the parent pointers back from that edge to the gray
  node it points to reconstructs the actual cycle, not just yes/no.
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

$ ./graph dfs examples/cycle.txt 0
preorder: 0 -> 1 -> 2
has_cycle: yes
cycle: 0 -> 1 -> 2 -> 0

$ ./graph bellmanford examples/negative.txt 0 3
distance: 4 weight
path: 0 -> 2 -> 1 -> 3

$ ./graph bfs /no/such/file.txt 0 1
error: cannot open /no/such/file.txt

$ ./graph toposort examples/dag.txt
order: 0 -> 1 -> 2 -> 3 -> 4

$ ./graph toposort examples/cycle.txt
CYCLE DETECTED: no topological order exists
```

`examples/weighted.txt` deliberately includes a direct 0->4 edge of weight
10 alongside the cheaper 0-1-2-4 path (weight 4), so Dijkstra's answer is
verifiably not just "the first path found". `examples/negative.txt`
similarly makes Bellman-Ford's negative-edge handling verifiable: the
direct-looking 0-1-3 path (weight 9) loses to 0-2-1-3 (weight 4), which
only wins *because* of the -2 edge along the way.

## Experiments

**Does Dijkstra's heap overhead show up as graphs grow, relative to BFS —
and does that depend on size, density, or which algorithm it's compared
against?** `src/bench.cpp` runs three experiments, all on random graphs
built from a fixed seed (graph construction excluded from all timings).

Real output from `make benchmark`:

```text
Benchmark: BFS vs Dijkstra runtime by graph size
(random graph, average out-degree 4, src=0, dst=n-1)

V         E           BFS (ms)      Dijkstra (ms)
100       393         0.003         0.011
1000      3997        0.025         0.148
10000     39996       0.281         1.770
50000     199995      1.109         14.053

Benchmark: does graph *density* (not just size) widen the BFS/Dijkstra gap?
(fixed V=2000, src=0, dst=V-1, average out-degree varies)

avg_deg   E           BFS (ms)      Dijkstra (ms) ratio
2         3998        0.030         0.169         5.5
4         7998        0.032         0.281         8.7
8         15990       0.043         0.391         9.0
16        31980       0.073         0.539         7.4
32        63957       0.088         0.669         7.6

Benchmark: Dijkstra vs Bellman-Ford runtime by graph size
(same random graphs as the first table -- both non-negative, so both give
 the same answer; this measures O((V+E)logV) vs O(V*E) directly)

V         E           Dijkstra (ms)   Bellman-Ford (ms)
100       393         0.010           0.007
1000      3997        0.132           0.070
5000      19993       0.797           0.419
10000     39996       1.686           0.872
```

## Results

**Size:** from 100 to 50,000 vertices (500x), BFS's runtime grew about
370x while Dijkstra's grew about 1280x — BFS does O(1) work per edge
(queue push/pop), Dijkstra does O(log V) (heap push, plus stale-entry
checks), so its relative cost grows with graph size, matching the
O(V+E) vs O((V+E) log V) bounds.

**Density:** the BFS/Dijkstra ratio isn't flat as average out-degree
grows at fixed V=2000 — it climbs from 5.5x (out-degree 2) to a peak
around 9x (out-degree 8), then settles back to ~7-8x at higher
densities rather than climbing indefinitely. The heap's `log V` factor
doesn't depend on density; what changes is how much of each algorithm's
per-edge work is cache-friendly sequential access (BFS's queue) versus
heap-node jumps (Dijkstra's `priority_queue`).

**Dijkstra vs. Bellman-Ford — a genuine surprise:** Bellman-Ford is
*faster* here at every size tested, not slower, despite worse
asymptotic complexity (O(V·E) vs O((V+E) log V)). On these graphs
(sparse, E ~ 4V), V·E and (V+E) log V are close enough that constant
factors decide it — Bellman-Ford's inner loop is a flat scan over a
`vector<Edge>` (sequential, cache-friendly, no allocation), while
Dijkstra pays for `priority_queue` push/pop and skips stale heap
entries on every pop. This isn't "Bellman-Ford is better" (it still
needs up to `V-1` full passes, and does lose badly on denser or larger
graphs than tested here) — it's that a Big-O comparison only predicts
who wins in the limit, and the limit can be further out than it looks.

## What I learned

The complexity-theory gap between BFS and Dijkstra is easy to state and
easy to forget matters in practice; here it's directly visible as a
widening runtime gap on the same hardware, same graphs, same code style.
The Bellman-Ford result was the bigger lesson: it would have been easy to
assume Dijkstra "must" win because its complexity bound is better, print a
benchmark table without really reading it, and never notice the numbers
actually said otherwise — real measurement caught an assumption a purely
theoretical comparison wouldn't have.

## Limitations

- Graphs are directed only; undirected graphs must be encoded as two
  opposite directed edges.
- No weighted topological sort / longest-path-in-DAG variant.
- Bellman-Ford detects a negative cycle reachable from `src` and throws
  rather than returning a wrong distance, but doesn't report *which*
  vertices are on it (unlike `dfs_find_cycle`, which does for ordinary
  cycles).
