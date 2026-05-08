# DSA 12 — Shortest paths: Dijkstra, Bellman-Ford, A*

> "Find the cheapest route from A to B." Maps, packet routing, game AI, build pipelines.

## Single-source shortest path

Given a weighted graph and source `s`, find the shortest distance from `s` to every other vertex.

### Dijkstra's algorithm — non-negative weights

Greedy: always extract the unvisited vertex with smallest tentative distance; relax its neighbors.

```
dist[s] = 0; dist[others] = ∞
push (0, s) to priority queue
while queue not empty:
    (d, u) = pop min
    if d > dist[u]: continue            # stale
    for (v, w) in adj[u]:
        if dist[u] + w < dist[v]:
            dist[v] = dist[u] + w
            push (dist[v], v)
```

With a binary heap: O((V + E) log V).
With a Fibonacci heap: O(E + V log V), but constants kill it.

```c
typedef struct { int v; int d; } pq_entry_t;   // priority by d
// Use the heap from DSA-10.

void dijkstra(int n, edge_t **adj, int s, int *dist) {
    for (int i = 0; i < n; i++) dist[i] = INT_MAX;
    dist[s] = 0;
    // ... pq setup ...
    while (pq_size) {
        pq_entry_t e = pq_pop();
        if (e.d > dist[e.v]) continue;
        for (edge_t *p = adj[e.v]; p; p = p->next) {
            int nd = dist[e.v] + p->w;
            if (nd < dist[p->to]) {
                dist[p->to] = nd;
                pq_push((pq_entry_t){p->to, nd});
            }
        }
    }
}
```

**Limitation**: Dijkstra **fails on negative edge weights**. It can settle a vertex too early.

### Bellman-Ford — handles negative weights

Relax every edge `V−1` times. If anything still improves on iteration `V`, there's a negative cycle.

```
for V-1 iterations:
    for each edge (u, v, w):
        if dist[u] + w < dist[v]:
            dist[v] = dist[u] + w
```

O(V·E). Slower, but more robust. Used in:
- distance-vector routing protocols (RIP);
- detection of arbitrage in currency exchange (negative cycle = profit loop);
- when negative weights model debits/credits.

### Floyd-Warshall — all-pairs

O(V³). Three nested loops:

```
for k in V: for i in V: for j in V:
    dist[i][j] = min(dist[i][j], dist[i][k] + dist[k][j])
```

Use for small graphs only. The simplicity is deceptive — works on negative weights, doesn't survive negative cycles.

## A* — heuristic-guided search

Like Dijkstra but uses a **heuristic** `h(u)` estimating distance from `u` to goal. Push priority = `dist[u] + h(u)`. If `h` is **admissible** (never overestimates), A* finds the optimal path.

Used in:
- video games (path on a tile map; `h` = Manhattan or Euclidean distance);
- robot navigation;
- puzzle solvers.

```c
// Same as Dijkstra, but priority is dist[u] + h(u).
```

If `h` is the actual perfect distance, A* visits a single line. If `h = 0`, A* degenerates to Dijkstra. The art is finding a tight admissible heuristic.

## Bidirectional search

For shortest path between two specific points: search from both ends; meet in middle. Often √n improvement. Used in route planners (Google Maps, OSM-routing).

## When to use what

| Situation | Algorithm |
|---|---|
| Non-negative weights, single source | Dijkstra |
| Negative weights, single source | Bellman-Ford |
| All pairs, small V | Floyd-Warshall |
| Single src/dst, good heuristic available | A* |
| Single src/dst, no heuristic, want speedup | Bidirectional Dijkstra |
| Unweighted | BFS |
| DAG with topo order | Single pass after topo sort — O(V+E) |

## Negative cycles

If a graph has a reachable negative-cost cycle, "shortest path" is undefined (you can loop forever to drive cost to −∞). Bellman-Ford detects this on iteration `V`. Always check.

## Practical issues

### Tie-breaking
When two priorities are equal, your priority queue may return them in any order. If your downstream code is order-sensitive, add a tie-breaker to the priority key.

### Numerical stability
Floats and shortest paths don't mix. Use ints (or scaled ints) when possible. With floats, two equal paths can differ by ε and cause flapping.

### Stale heap entries
Standard binary heap doesn't support decrease-key well. Common idiom: just push the new (smaller) entry; on pop, skip if stale (`if e.d > dist[e.v] continue`). Heap may grow to O(E) — fine.

## In the real world

- **Vulkan/DX12 dependency graphs** — bird's-eye, jobs depend on jobs. The compiler's barrier inserter walks the graph.
- **Linux's deadline scheduler** — not exactly shortest path, but uses graph reasoning on temporal constraints.
- **PCIe topology** — Linux's PCI topology is a tree; "shortest path" is trivial. But device-affinity tools use distance metrics.
- **Memory access** in NUMA — the kernel's NUMA distance table is essentially used to route memory requests via shortest distance.

## Common bugs

1. Using Dijkstra with negative weights — **silently wrong**.
2. Forgetting the "stale entry skip" check — explosive runtime.
3. Wrong heuristic in A* (overestimating) — gives non-optimal paths.
4. Integer overflow when summing weights for huge graphs — use `long long` or saturate.
5. Bad initialization (`INT_MAX + w` overflows — guard before adding).

## Try it

1. Implement Dijkstra with a binary heap. Test on a hand-drawn graph.
2. Implement Bellman-Ford and detect a negative cycle (construct one).
3. Implement A* on a 2D grid with obstacles. Heuristic: Manhattan distance. Compare visited node counts vs Dijkstra (`h = 0`).
4. Read a city map (e.g., a small CSV of edges) and find the shortest route between two named nodes.
5. Detect a negative cycle in a currency exchange graph (vertices = currencies, edge `(u, v)` = `−log(rate(u→v))`). If shortest path from `u` to `u` is negative, there's arbitrage.
6. Bonus: implement bidirectional Dijkstra. Compare nodes visited vs unidirectional.

## Read deeper

- CLRS chapter 24 (Single-source) and 25 (All-pairs).
- "Contraction Hierarchies" — modern technique used by Google Maps for super-fast routing.
- Russell & Norvig *AI: A Modern Approach* — chapter on heuristic search.

Next → [Tries](DSA-13-tries.md).
