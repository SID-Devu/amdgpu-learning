# DSA 26 — System-design data structures

> LRU cache, bloom filter, skip list, consistent hashing — the data structures that show up in senior-level "system design" interviews and in real distributed/cache systems.

## LRU cache

**Need**: cap memory at N entries; on overflow, evict the **least recently used**.

**O(1) get/put** using a hash map + doubly linked list.

```
hash map: key → list node (in O(1))
doubly linked list: head = MRU, tail = LRU (insert/remove in O(1) with known node)
```

```c
typedef struct lru_node {
    int key, val;
    struct lru_node *prev, *next;
} lru_node_t;

typedef struct {
    int cap, size;
    lru_node_t *head, *tail;        // dummy nodes
    hmap_t map;                     // key -> lru_node_t*
} lru_t;

void lru_get(lru_t *l, int key, int *out) {
    lru_node_t *n;
    if (hmap_get(&l->map, key, &n) == 0) {
        // remove from current pos, move to head
        n->prev->next = n->next;
        n->next->prev = n->prev;
        n->next = l->head->next;
        n->prev = l->head;
        l->head->next->prev = n;
        l->head->next = n;
        *out = n->val;
    } else *out = -1;
}

void lru_put(lru_t *l, int key, int val) {
    // similar; if at cap, evict tail->prev
    ...
}
```

**Used in**: page caches, CPU caches (the policy, simplified), Memcached, Redis (LRU is one of several eviction policies), Linux's dentry cache (with twist), web caches.

**Variant**: LFU (least frequently used), LRU-K, ARC (Adaptive Replacement Cache used by ZFS).

## LFU cache (frequency-based)

Counts accesses; evicts the least-accessed. More state; trickier to make O(1). Done with a list of "frequency buckets" + per-bucket DLLs.

## Bloom filter

**Need**: "have we seen this key?" with tiny memory and acceptable false positives.

- **Bit array** of size m, all zero initially.
- **k hash functions** mapping keys to indices.
- Insert: set the k bits.
- Query: check the k bits. **All set** → maybe present (false positive possible). **Any unset** → definitely not.

```c
typedef struct { unsigned long *bits; size_t m; int k; } bloom_t;

void bloom_add(bloom_t *b, const char *key) {
    for (int i = 0; i < b->k; i++) {
        unsigned long h = hash_with_seed(key, i) % b->m;
        b->bits[h / 64] |= 1UL << (h % 64);
    }
}

int bloom_maybe(bloom_t *b, const char *key) {
    for (int i = 0; i < b->k; i++) {
        unsigned long h = hash_with_seed(key, i) % b->m;
        if (!(b->bits[h / 64] & (1UL << (h % 64)))) return 0;
    }
    return 1;
}
```

For n elements and false positive rate p: optimal `m = -n ln p / (ln 2)^2` bits, `k = (m/n) ln 2`.

**Used in**: BigTable / Cassandra / RocksDB (skip disk reads when key definitely absent), web crawlers (don't recrawl), spellcheck dictionaries, network packet sampling.

**Cannot delete** in classic bloom filter. **Counting bloom filter** uses small counters instead of bits → can decrement on delete. Trade memory for delete support.

## Skip list

A randomized data structure giving O(log n) expected search/insert/delete with simpler code than balanced trees. Used in: LevelDB / RocksDB memtable, Java's `ConcurrentSkipListMap`, Redis sorted sets.

Idea: linked list with "express lanes" at randomized higher levels. Each node has a height geometrically distributed.

```
level 3:  head ——————————————————> NULL
level 2:  head ——> 4 ——————> 16 ——> NULL
level 1:  head -> 2 -> 4 -> 8 -> 16 -> NULL
```

Search: start at top level; move right while next < target; drop down. Average path length ~log n.

Skip lists are **lock-free-friendly** — easier than balanced BSTs in concurrent settings.

## Consistent hashing

**Problem**: distribute keys across N nodes such that adding/removing a node only **moves a fraction** of keys (not all of them).

**Naive**: `hash(key) % N`. If N changes from 3 to 4, almost every key moves. Bad.

**Consistent hashing**: hash both nodes and keys onto a circle (`[0, 2^32)`). A key belongs to the next node clockwise.

```
positions on a 32-bit ring:
    node A at 100
    node B at 1_000_000
    node C at 3_000_000
key  hash 500_000 → goes to node B
```

Adding node D at 700_000: only keys between B's predecessor and 700_000 move from B to D. Only 1/N of keys move on average.

**Virtual nodes**: each physical node hashed to many positions (200+) → smoother distribution.

**Used in**: DynamoDB, Cassandra, Memcached client libraries, content-delivery networks, sharded systems.

**Variants**:
- **Rendezvous hashing** (HRW) — `for each node n, compute hash(key, n); pick the node with max hash`. Handles "weights" naturally; no ring.
- **Jump consistent hashing** (Lamping & Veach 2014) — small constant time, no metadata. Used by some databases for shard placement.

## Skiplist vs B-tree vs RB-tree (for indexes)

| Property | Skiplist | B+tree | RB-tree |
|---|---|---|---|
| Concurrency-friendly | Yes | Tricky | Per-node lock OK |
| Cache-friendly | Mediocre | Yes (block-sized nodes) | Mediocre |
| Disk-friendly | No | Yes | No |
| Complexity | Simple | Complex | Mid |

For in-memory + concurrent → skiplist. For on-disk → B+tree. For pure single-thread in-memory → RB-tree (low constants).

## Reservoir sampling

Choose K random items uniformly from a stream of unknown length without storing all of them.

```c
// R1: keep first k. for i = k+1..n: choose to replace with prob k/i.
for (int i = 0; i < n; i++) {
    if (i < k) reservoir[i] = a[i];
    else {
        int j = rand() % (i + 1);
        if (j < k) reservoir[j] = a[i];
    }
}
```

Used in: log sampling, A/B testing, distributed counting.

## Count-min sketch

Approximate frequency of items in a stream with sublinear memory. d hash functions, w buckets each. Insert: increment all `cms[j][h_j(x)]`. Query: take the **min** across the row of d.

Bias: never undercounts; can overcount. Trade memory for accuracy.

Used in: heavy-hitter detection, network telemetry.

## HyperLogLog

Estimate the **cardinality** (number of distinct items) of a stream in tiny memory.

Idea: count leading zeros of hashes; longer streaks suggest more distinct items. Aggregate via "buckets" of m registers; harmonic mean.

Memory: ~12 KB for billions of items with ~1% error.

Used in: Google's pageview estimation, Redis's `PFCOUNT`.

## In Linux / kernel-adjacent

- **Page cache**: LRU (with twists — active/inactive lists).
- **Slab allocator**: sometimes uses bloom-filter-like tricks for fast presence checks.
- **DRBD, GlusterFS, Ceph**: consistent hashing for shard placement.
- **fscrypt** key cache: LRU.

## Common bugs

1. **LRU**: forgetting to move on every access; only moving on writes.
2. **Bloom filter**: not enough hash functions → high false positives. Or using non-independent hashes (XOR of similar hashes).
3. **Consistent hashing**: not enough virtual nodes → uneven distribution.
4. **Skip list**: wrong probability for level → degenerate to linked list.
5. **Reservoir sampling**: getting probabilities wrong → biased sampling.

## Try it

1. Implement an LRU cache (hash + DLL) with `get` and `put` in O(1). Test eviction on cap=3.
2. Implement a bloom filter; test with 100K inserted keys; measure false-positive rate on 100K random not-inserted keys.
3. Implement a skip list with `insert`, `find`, `delete`. Compare to a BST.
4. Implement consistent hashing with V virtual nodes per physical node. Show that moving 1 node moves ~1/N of keys.
5. Implement reservoir sampling. Sample 100 items from a 10000-item stream; verify uniformity over many runs.
6. Bonus: HyperLogLog. Compare to a hash set on a 1M-cardinality stream; measure memory.

## Read deeper

- "Bloom filters in probabilistic verification" — Bloom's original paper.
- Karger et al., "Consistent Hashing and Random Trees: Distributed Caching Protocols" (the Akamai paper).
- "HyperLogLog: the analysis of a near-optimal cardinality estimation algorithm" — Flajolet et al.
- *Designing Data-Intensive Applications* by Kleppmann — strong on LSM, replication, consensus.

Next → [Concurrency-aware DS: lock-free queue, RCU, hazard pointers](DSA-27-concurrent-ds.md).
