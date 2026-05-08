# DSA 23 — Union-Find / Disjoint-Set Union (DSU)

> A tiny data structure that supports "are these two things in the same group?" and "merge two groups." Used in Kruskal's MST, network connectivity, image segmentation.

## Operations

- `make_set(v)` — start a new singleton.
- `find(v)` — return the representative of v's set.
- `union(a, b)` — merge the two sets.

If implemented well, both operations are **near-constant**: amortized `O(α(n))` where `α` is the inverse Ackermann function — practically `≤ 4` for any n you'll see.

## Implementation

Each node points to a "parent." The root is the representative.

```c
int parent[N], rank[N];

void make_set(int v) { parent[v] = v; rank[v] = 0; }

int find(int v) {
    if (parent[v] == v) return v;
    return parent[v] = find(parent[v]);   // path compression
}

void union_sets(int a, int b) {
    a = find(a); b = find(b);
    if (a == b) return;
    if (rank[a] < rank[b]) { int t = a; a = b; b = t; }
    parent[b] = a;
    if (rank[a] == rank[b]) rank[a]++;
}
```

Two optimizations make it fast:

- **Path compression**: `find(v)` flattens the tree by repointing every node on the path to the root.
- **Union by rank** (or **by size**): always attach the shorter tree under the taller. Keeps trees flat.

Either alone gives `O(log n)` amortized; both together give `O(α(n))`. Always use both.

## Kruskal's MST

Given a graph with weighted edges:

1. Sort edges ascending by weight.
2. For each edge `(u, v)`: if `find(u) != find(v)`, add to MST and union.
3. Stop when MST has `V-1` edges.

```c
qsort(edges, n_edges, sizeof(edge), cmp_by_weight);
for (int i = 0; i < N; i++) make_set(i);
int total = 0, taken = 0;
for (int i = 0; i < n_edges && taken < N - 1; i++) {
    int u = edges[i].u, v = edges[i].v;
    if (find(u) != find(v)) {
        union_sets(u, v);
        total += edges[i].w;
        taken++;
    }
}
```

O(E log E) — sort dominates.

## Other applications

### Network connectivity
"Are nodes A and B connected?" → `find(A) == find(B)`. Online queries with edge insertions: O(α) per op.

### Image segmentation
Pixels are nodes; merge adjacent pixels with similar colors. Output "regions" = connected components.

### Cycle detection in undirected graphs
While processing edges, if both endpoints already in the same set, this edge creates a cycle.

### Equivalence classes
Anywhere you need "treat these as the same."

### Percolation
Statistical physics simulation: random connections; ask "is the top connected to the bottom?"

### Compiler optimizations
SSA construction's phi insertion; type unification in Hindley-Milner.

## Variants

### DSU with weights / "potentials"
Each node tracks a value relative to its root. Useful for: "given equations `a − b = c`, are they consistent?"

### Persistent DSU
Snapshot history of merges; query historical state. More complex; useful for offline queries.

### Online dynamic connectivity (deletions allowed)
Much harder. Uses link-cut trees or Holm-Lichtenberg-Thorup.

## In the kernel

DSU isn't a stock kernel data structure (no `disjoint_set.h`). But the **idea** appears in:

- block-merging in some allocator coalescing,
- ext4's group descriptor management (sort of),
- network connectivity tests.

In userspace tools (e.g., `lvm`, `multipath`), DSU is more directly used.

## Common bugs

1. Forgetting **path compression** → trees can become long.
2. **Union by rank wrong** when sizes/ranks flip — careful update of rank.
3. **Recursive find** stack overflow on huge inputs — convert to iterative.
4. **Counting components** wrong: maintain a counter, decrement on every successful union.

## Iterative find (avoids deep recursion)

```c
int find_iter(int v) {
    int r = v;
    while (parent[r] != r) r = parent[r];
    while (parent[v] != r) {
        int next = parent[v];
        parent[v] = r;
        v = next;
    }
    return r;
}
```

## Try it

1. Implement DSU with both path compression and union by rank.
2. Implement Kruskal's MST. Test on a small graph; verify against Prim's.
3. Solve "Number of connected components in undirected graph" (Leetcode 323).
4. Solve "Accounts merge" (Leetcode 721) — DSU on email addresses.
5. Bonus: simulate percolation. Random NxN grid; open cells progressively until top connects to bottom; report how many cells were opened.

## Read deeper

- CLRS chapter 21 (Disjoint Sets).
- Tarjan's analysis of `α(n)` — beautiful, hard.
- *Competitive Programmer's Handbook* — concise DSU section.

Next → [Advanced trees: segment, Fenwick, interval](DSA-24-advanced-trees.md).
