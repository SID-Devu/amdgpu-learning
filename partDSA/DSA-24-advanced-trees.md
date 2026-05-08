# DSA 24 — Advanced trees: segment, Fenwick (BIT), interval

> When you need range queries plus point updates in O(log n), reach for these.

## The problem they solve

Given an array, support:
- **Point update**: change `a[i]`.
- **Range query**: e.g., sum / min / max over `a[l..r]`.

Naive: O(n) per query (just scan). Both segment trees and Fenwick trees give **O(log n)** for both ops.

## Fenwick tree (Binary Indexed Tree, BIT)

A clever array-based structure that supports prefix-sum queries and point updates in O(log n).

```c
int bit[N + 1];   // 1-indexed

void update(int i, int delta) {
    for (; i <= N; i += i & -i) bit[i] += delta;
}

int prefix_sum(int i) {
    int s = 0;
    for (; i > 0; i -= i & -i) s += bit[i];
    return s;
}

int range_sum(int l, int r) {       // 1-indexed, inclusive
    return prefix_sum(r) - prefix_sum(l - 1);
}
```

The `i & -i` (lowest set bit, DSA-20) is the magic. Each iteration jumps to a higher index whose range covers a longer prefix.

- O(log n) per op.
- O(n) space.
- Tiny code (4 lines). Often beats segment tree in constants.
- Limited: fundamentally about **prefix-aggregate** ops where a "range" can be expressed as "prefix(r) − prefix(l-1)" — that means **invertible** ops (sum, XOR, count). Not min/max (no good "subtract" operation).

## Segment tree

A balanced binary tree where each node stores a summary of an interval.

Layout: heap-style array of size 4n.

```c
int tree[4 * N];

// build, query, update

void build(int node, int l, int r, int *a) {
    if (l == r) { tree[node] = a[l]; return; }
    int m = (l + r) / 2;
    build(2*node, l, m, a);
    build(2*node+1, m+1, r, a);
    tree[node] = tree[2*node] + tree[2*node+1];
}

void update(int node, int l, int r, int idx, int val) {
    if (l == r) { tree[node] = val; return; }
    int m = (l + r) / 2;
    if (idx <= m) update(2*node, l, m, idx, val);
    else          update(2*node+1, m+1, r, idx, val);
    tree[node] = tree[2*node] + tree[2*node+1];
}

int query(int node, int l, int r, int ql, int qr) {
    if (qr < l || r < ql) return 0;             // outside
    if (ql <= l && r <= qr) return tree[node];  // fully inside
    int m = (l + r) / 2;
    return query(2*node, l, m, ql, qr) + query(2*node+1, m+1, r, ql, qr);
}
```

- O(n) build, O(log n) query and update.
- More flexible than BIT: can handle **min, max, GCD, "count of zeros," etc.**
- Larger code, larger memory, slightly slower constants.

### Lazy propagation
For **range updates** (e.g., "add 5 to all of [l, r]"): mark each node lazily; defer to descendants until needed. O(log n) for both range update and range query. Much harder to implement correctly. Reach for it when needed.

## Interval tree

A red-black tree augmented with "max endpoint in subtree." Supports: "given an interval [a, b], find all stored intervals that overlap it."

Operations: insert/delete in O(log n); single-overlap query in O(log n); all-overlap query in O(k + log n).

The Linux kernel's `interval_tree.h` is exactly this — used in:
- KVM page-tracking,
- DRM/GPUVM range tracking,
- Btrfs free-space tracking,
- mmu_notifiers.

```c
// see include/linux/interval_tree.h and lib/interval_tree.c
struct interval_tree_node {
    struct rb_node rb;
    unsigned long start, last;
    unsigned long __subtree_last;     // augmentation: max(last) over subtree
};
```

Read this file when you reach Part VII (GPUVM uses it heavily).

## Range tree
For 2D and higher: tree of trees. Useful in computational geometry. Out of scope for this book; mention it exists.

## kd-tree
Spatial partitioning for k-dimensional points. Used in nearest-neighbor search, ray tracing acceleration structures (BVH is similar), graphics. We'll see BVH in Part X (ray tracing).

## When to use what

| Need | Choice |
|---|---|
| Prefix sum / point update | Fenwick tree |
| Range query (any), point update | Segment tree |
| Range update + range query | Segment tree with lazy propagation |
| "Find overlapping interval" | Interval tree (RB-augmented) |
| Spatial nearest-neighbor | kd-tree, BVH |
| Just one query type, n small | Just an array — don't over-engineer |

## In the kernel

- `include/linux/interval_tree.h` — used in mmu_notifiers and elsewhere.
- `lib/interval_tree.c` — read this; one of the cleanest examples of the augmented RB-tree pattern.
- Btrfs uses interval trees for free-space management.
- Out-of-tree GPU drivers use them for VA range tracking.

## Common bugs

1. **Off-by-one** with 0- vs 1-indexed in BIT.
2. **Forgetting** to recompute parent in segment tree update.
3. **Inclusive/exclusive** boundary inconsistency in range queries.
4. **Memory blow-up**: 4n for segment tree (some say 2*next_pow2(n)).
5. **Lazy propagation** forgetting to push down before descending.

## Try it

1. Implement BIT for prefix-sum + point update. Test against naive on random ops.
2. Implement segment tree for "range max" with point updates.
3. Implement segment tree with lazy propagation for "add x to range" + "sum range."
4. Implement an interval tree (augmented RB-tree). Insert intervals; given a query interval, find all overlapping ones.
5. Read `lib/interval_tree.c`. Trace one insertion by hand on paper.
6. Bonus: solve "Range Sum Query Mutable" on Leetcode using BIT.

## Read deeper

- *Competitive Programmer's Handbook* by Antti Laaksonen — best free intro to these.
- "Augmenting Data Structures" — CLRS chapter 14.
- "Persistent segment trees" if you want to go deep.

Next → [String algorithms: KMP, Rabin-Karp, Z, suffix array](DSA-25-string-algos.md).
