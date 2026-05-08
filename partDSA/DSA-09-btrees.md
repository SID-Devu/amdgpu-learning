# DSA 9 — B-trees and B+trees

> The data structure that runs your filesystem and your database.

## Why not just use a red-black tree?

Red-black trees are great in RAM. But:

- A disk seek is ~100,000× slower than a RAM access.
- A red-black tree of 1 billion entries is 30 levels deep → up to 30 disk seeks per lookup.
- Cache lines and disk blocks are **4 KB** (or 8/16/64 KB). Reading 24 bytes wastes the rest.

**B-tree idea**: each node holds many keys (e.g., 100–500). Tree fanout is huge → tree depth is tiny (e.g., 4 levels for a billion entries). Each node fills exactly one disk block.

## B-tree definition

A B-tree of order `m`:

- Each node has between `⌈m/2⌉` and `m` children (except the root).
- Each node has between `⌈m/2⌉ - 1` and `m - 1` keys.
- All leaves at the same depth.
- Keys within a node are sorted; subtrees between consecutive keys hold values in that range.

Example, order 4 ("2–3–4 tree"):

```
                [10 | 20]
              /     |     \
        [3,7]   [13,17]   [25,30]
```

A search for `15`: at root, `10 < 15 < 20` → go to middle child → at `[13, 17]`, `13 < 15 < 17` → between → leaf → not found.

Each step moves down a level — O(log_m n) levels.

## Operations

### Search
- Inside a node, binary search for the key.
- Go to the appropriate child if not found at this level.

### Insert
- Always insert at a leaf.
- If the leaf overflows (`m` keys), **split**: middle key goes up to parent.
- If parent overflows, recurse upward. Sometimes the root splits → tree grows by 1.

### Delete
- Find key.
- If at internal node, swap with predecessor or successor (now in a leaf).
- Delete from leaf.
- If leaf underflows, **borrow** from sibling, or **merge** with sibling.
- Recurse upward; root may shrink.

Insert and delete are involved (lots of cases). Most engineers **read** the algorithm rather than reimplement it.

## B+tree

A variation:

- All values stored **only at leaves**. Internal nodes hold only keys for routing.
- **Leaves are linked in a list** → sequential / range queries are fast.

```
                  Internal: routing keys only
                  /            |              \
        [leaves linked: ←→ ←→ ←→ ←→  containing actual records]
```

This is what almost all modern databases and filesystems actually use:
- **InnoDB** (MySQL): B+tree per index.
- **PostgreSQL**: B-tree by default for indexes (technically Lehman-Yao, a concurrent B+tree variant).
- **ext4**: directory entries are stored in a B-tree-ish structure (Htree).
- **xfs, btrfs**: B+tree based.
- **ZFS**: B-tree variants.

## Cache and performance

The right node size is "1 cache line" for in-RAM B-trees, "1 block" for disk. Modern CPU L1 line is 64B; L2 is 1MB; B-tree variants exist that exploit cache hierarchy (B-trees of B-trees, "fractal trees"). For most database work, **B-tree of pages** (a page = 4–16 KB) is canonical.

## Concurrency

Naive B-tree concurrency: lock the tree. Bad — every operation serializes.

**Lehman-Yao** B+tree (used by Postgres): allow concurrent readers and writers via "high-key" pointers and per-node latches. Read paths are mostly lock-free; writers latch only nodes they touch.

This level of detail is beyond DSA basics, but you should know it exists.

## In the kernel / on disk

- `fs/btrfs/ctree.c` — btrfs's "copy-on-write B-tree." Read this if you want filesystem mastery.
- `Documentation/filesystems/btrfs.rst`.
- ext4 directory hashing (`fs/ext4/dir.c`).

## When B-trees aren't right

- For pure RAM, in-process — RB-tree or hash beats B-tree. (Cache locality of B-tree helps mainly when nodes are bigger than a line; RAM B-tree of 16-key nodes is great too — see "B-tree" by Bender et al.)
- For append-only workloads — LSM trees (RocksDB, LevelDB) win.
- For point lookups where you don't need sorted iteration — hash beats both.

## LSM trees (briefly, for completeness)

Log-Structured Merge tree:
- Writes go into a small in-RAM sorted structure (skip list).
- When full, flushed to disk as a sorted "SSTable."
- Older SSTables periodically **compacted** (merge-sorted) into bigger ones.

Optimizes writes; pays at read time (look in multiple levels). Used in: RocksDB, LevelDB, Cassandra, HBase, Bigtable.

## Try it (you don't need to write a full B-tree)

1. Read the Wikipedia "B-tree" article. Draw the insertion of `1..7` into an order-3 B-tree. Confirm the tree shape after each insert.
2. Implement a 2-3 tree (B-tree of order 3) — only ~150 lines if you're patient, and it's a great exercise. Test by inserting 1..1000 and verifying invariants.
3. Read `Documentation/filesystems/btrfs.rst`. Identify how btrfs uses copy-on-write to avoid in-place writes.
4. Compare RB-tree vs B-tree of small order in RAM on 10 million inserts. Time both. Comment on cache effects.
5. Bonus: read about **fractional cascading** — a technique to speed up multi-tree searches.

## Read deeper

- CLRS chapter 18 (B-trees).
- Lehman & Yao (1981) "Efficient locking for concurrent operations on B-trees" — the Postgres approach.
- Modern: "The Bw-Tree: A B-tree for new hardware" (Microsoft Research) for lock-free B-tree variants.

Next → [Heaps & priority queues](DSA-10-heaps.md).
