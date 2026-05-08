# DSA 11 — Graphs: representation & traversal

> A graph is "things and the relationships between them." The internet is a graph. The kernel's wait-for-fence dependency is a graph. The DRM scheduler's dependency tree is a graph. Master traversal once.

## Definitions

- **Graph** `G = (V, E)` — a set of vertices and edges.
- **Directed** vs **undirected**.
- **Weighted** edges have costs.
- **Cyclic** vs **acyclic**.
- **DAG** — directed acyclic graph. Used for build systems, schedulers, dependency resolution.
- **Connected** (undirected) — every pair of vertices has a path between them.
- **Strongly connected** (directed) — every pair has a directed path both ways.

## Representations

Two main, plus a few others.

### Adjacency matrix
`adj[u][v] = 1` if there's an edge from `u` to `v` (or weight, or 0).

```c
int adj[N][N];
```

- Space: O(V²). Bad for sparse graphs.
- Edge query `(u,v)`: O(1).
- Iterate u's neighbors: O(V).

Good for **dense graphs** or **fast edge lookups** (e.g., chess board adjacency).

### Adjacency list
For each vertex, a list of its neighbors.

```c
typedef struct edge { int to; int w; struct edge *next; } edge_t;
edge_t *adj[N];

void add_edge(int u, int v, int w) {
    edge_t *e = malloc(sizeof(*e));
    e->to = v; e->w = w; e->next = adj[u];
    adj[u] = e;
}
```

- Space: O(V + E).
- Edge query `(u,v)`: O(degree(u)).
- Iterate u's neighbors: O(degree(u)).

**Good for sparse graphs** (most real graphs).

### CSR (compressed sparse row) — read-mostly graphs

Two flat arrays: `offsets[v]` to `offsets[v+1]` gives a slice of `targets[]`. Hugely cache-friendly. Used in graph databases, Pregel-style frameworks.

## Breadth-First Search (BFS)

Explore in waves. Uses a **queue**.

```c
void bfs(int s) {
    int visited[N] = {0};
    int q[N], head = 0, tail = 0;
    q[tail++] = s; visited[s] = 1;
    while (head < tail) {
        int u = q[head++];
        printf("visited %d\n", u);
        for (edge_t *e = adj[u]; e; e = e->next) {
            if (!visited[e->to]) {
                visited[e->to] = 1;
                q[tail++] = e->to;
            }
        }
    }
}
```

- O(V + E) time and space.
- **BFS finds shortest path in unweighted graphs.**
- Use cases: shortest path on grids, level-order tree traversal, friend-of-friend networks, web crawl, peer-to-peer broadcast.

## Depth-First Search (DFS)

Explore deeply, then backtrack. Uses recursion (or an explicit stack).

```c
int visited[N];
void dfs(int u) {
    visited[u] = 1;
    printf("visited %d\n", u);
    for (edge_t *e = adj[u]; e; e = e->next)
        if (!visited[e->to]) dfs(e->to);
}
```

- O(V + E) time and space.
- Iterative version uses a stack; helpful for deep graphs that overflow recursion.
- Use cases: cycle detection, topological sort, finding strongly connected components, maze solving, backtracking.

## What DFS unlocks

### Cycle detection (directed graph)
Three colors: white (unvisited), gray (in stack), black (done). A "back edge" to a gray node = cycle.

```c
int color[N];
int has_cycle(int u) {
    color[u] = 1;
    for (edge_t *e = adj[u]; e; e = e->next) {
        if (color[e->to] == 1) return 1;            // back edge
        if (color[e->to] == 0 && has_cycle(e->to)) return 1;
    }
    color[u] = 2;
    return 0;
}
```

### Topological sort (DAG only)
"Order vertices so all edges go forward."

DFS variant: visit + emit on **post-order**, then reverse.

```c
int order[N], idx;
void topo(int u) {
    visited[u] = 1;
    for (edge_t *e = adj[u]; e; e = e->next)
        if (!visited[e->to]) topo(e->to);
    order[--idx] = u;   // fill from the back
}
```

Used in: build systems (Make, ninja), task schedulers, course prerequisites, **GPU dependency tracking** (a job depends on its ins; topo gives a valid execution order).

### Connected components
Repeated BFS/DFS until all vertices visited. Each pass marks one component.

### Strongly connected components (Tarjan / Kosaraju)
For directed graphs. Both run in O(V + E). Tarjan uses one DFS pass; Kosaraju uses two. Used in compiler analysis, social networks.

## Bipartite check (BFS/DFS coloring)
"Can vertices be 2-colored so no edge has same-colored endpoints?" Color via BFS; conflict ⇒ not bipartite. Used in scheduling, matching.

## Floyd-Warshall (for completeness)
All-pairs shortest paths in O(V³). Use only for small graphs (V ≤ ~500).

## In the kernel / GPU stack

- **DRM scheduler dependencies**: each `drm_sched_job` has a list of `dma-fences` it must wait on. Topological execution of jobs across rings. Cycle detection here would be a bug — each fence has a unique signal-once contract.
- **Module loading** (`kernel/module.c`): a topological sort over dependencies (`MODULE_DEVICE_TABLE` and friends).
- **systemd's unit graph** — runs services in dependency-respecting order.
- **DAG of compilation units** — Make, ninja, bazel.

You will write graph traversal **less often** than tree traversal, but when you do, it's usually high-impact.

## Common bugs

1. Forgetting to mark visited before queueing → infinite loop.
2. Marking visited only at dequeue → same node enqueued multiple times.
3. Using DFS on a graph too deep for the call stack — use iterative + stack.
4. Confusing directed and undirected: in undirected, add edge both ways.
5. Bad complexity (e.g., O(V²) when adjacency lists give O(V+E)).

## Try it

1. Implement an adjacency-list graph; load from a file with edges; BFS from vertex 0; print BFS order.
2. Detect a cycle in a directed graph using colors. Test on a known DAG and a known cyclic graph.
3. Topological sort. Test on a small dependency graph (e.g., "shoes need socks; pants don't").
4. Find connected components in an undirected graph.
5. Bipartite check on a graph; print the 2-coloring if bipartite.
6. Bonus: implement Tarjan's SCC. Run on a graph with two cycles connected by one edge; verify two SCCs.

## Read deeper

- CLRS chapters 22 (Elementary Graph Algorithms) and 23 (MST).
- Sedgewick *Algorithms* — beautiful graph chapter with diagrams.
- For real graph engines: GraphX, Pregel, GraphBLAS.

Next → [Shortest paths: Dijkstra, Bellman-Ford, A*](DSA-12-shortest-paths.md).
