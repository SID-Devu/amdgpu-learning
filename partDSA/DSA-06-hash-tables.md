# DSA 6 — Hash tables (read-to-learn version)

> The data structure that finds anything in **constant time** — by computing a magic number from the key and using it as an index.

## The big idea

Imagine you have 10,000 names and you want to look one up.

- **Array search**: scan all 10,000. **O(n).** Slow.
- **Sorted array + binary search**: ~14 lookups. **O(log n).** Faster.
- **Hash table**: usually 1 lookup. **O(1).** Fastest.

How? A **hash function** turns a key (like a name) into a number — the **bucket index**. We store the value in that bucket. To find it later, we compute the same number from the key and go straight there.

```
Key "alice"  ──hash──▶  number 173  ──% 10──▶  bucket 3
Key "bob"    ──hash──▶  number 8200 ──% 10──▶  bucket 0
Key "carol"  ──hash──▶  number 9931 ──% 10──▶  bucket 1

       buckets:
       0  ──▶ "bob"   = 25
       1  ──▶ "carol" = 30
       2  ──▶ (empty)
       3  ──▶ "alice" = 22
       ...
       9  ──▶ (empty)
```

To look up "alice": compute hash, modulo 10 → bucket 3 → value is 22. **One step.**

## What a hash function does

A hash function is any function that:
1. Takes a key (string, int, struct).
2. Returns a number — usually 32 or 64 bits.
3. Spreads keys **uniformly** across the number range.

A simple hash for a string (FNV-1a):

```c
unsigned int fnv1a(const char *s) {
    unsigned int h = 2166136261u;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 16777619u;
    }
    return h;
}
```

For each byte: XOR it with the running hash, then multiply by a magic prime. The result is a 32-bit number that "looks random" relative to the input.

Three keys, three different hashes:

| key | fnv1a hash |
|---|---|
| "alice" | 0x4f5cea64 (1,247,843,940) |
| "bob"   | 0xfc8a3a93 (4,236,635,795) |
| "carol" | 0x6cd66bf3 (1,825,701,875) |

Two important properties:

- Same key always hashes to the same number — **deterministic.**
- Tiny input changes → wildly different hash — **avalanche.** ("alice" vs "alicd" should look unrelated.)

## Bucket selection: hash modulo size

Once you have the hash, reduce it to your bucket count:

```c
bucket = hash % n_buckets;
```

For `fnv1a("alice") = 1,247,843,940`, with `n_buckets = 10`:

```
1,247,843,940 % 10 = 0
```

"alice" goes to bucket 0.

If `n_buckets` is a power of 2 (say 16):

```c
bucket = hash & (n_buckets - 1);     // faster than %
```

The bottom 4 bits of the hash become the bucket index.

## Collisions — when two keys land in the same bucket

Two different keys can hash to the same bucket. That's a **collision**. Even with great hash functions, collisions are inevitable: with 10 buckets and 11 keys, **at least two share a bucket** (pigeonhole).

Two main ways to handle them.

### Way 1: Chaining (linked list per bucket)

Each bucket holds a linked list of (key, value) pairs that hashed there. To insert: append to the bucket's list. To find: walk the list comparing keys.

```
buckets:

  0 ──▶ ["bob"=25] ──▶ ["dave"=99] ──▶ NULL    ← two keys collided here
  1 ──▶ ["carol"=30] ──▶ NULL
  2 ──▶ NULL
  3 ──▶ ["alice"=22] ──▶ NULL
  ...
```

If chains are short (1–2 entries), look-up is still O(1) on average. If chains grow long, look-up degrades toward O(n).

### Way 2: Open addressing (probe to next slot)

If the bucket is taken, try the next slot, and the next, until empty. To find later, probe the same way.

Picture: cap=10, "bob" at bucket 0. Now "dave" hashes to 0 too:

```
buckets:
  0:  "bob"=25
  1:  (empty) ← dave will land here

Insert "dave": probe 0 (taken), probe 1 (empty) → put dave here.
```

```
  0:  "bob"=25
  1:  "dave"=99
```

To **find "dave"**: hash → 0; check slot 0, key is "bob" not "dave"; probe to slot 1, found.

## Full example: chained hash table with 4 buckets

Insert 5 keys:

| key | hash (made up) | bucket = hash % 4 |
|---|---|---|
| "alice" | 11 | 3 |
| "bob" | 8 | 0 |
| "carol" | 17 | 1 |
| "dave" | 12 | 0 |
| "eve" | 5 | 1 |

After all inserts:

```
buckets:

  0 ──▶ ["bob"=...] ──▶ ["dave"=...] ──▶ NULL
  1 ──▶ ["carol"=...] ──▶ ["eve"=...] ──▶ NULL
  2 ──▶ NULL
  3 ──▶ ["alice"=...] ──▶ NULL
```

Look up "eve":
1. Hash "eve" → 5.
2. 5 % 4 = 1.
3. Walk bucket 1's list: first entry is "carol" — not eve. Next: "eve" — match!
4. Return its value.

Two pointer follows. **Approximately O(1).**

## The full code (chained hash table)

```c
typedef struct entry {
    char *key;
    int   val;
    struct entry *next;
} entry_t;

typedef struct {
    entry_t **buckets;
    size_t   n_buckets;
    size_t   size;
} hmap_t;

void hmap_init(hmap_t *h, size_t n) {
    h->buckets   = calloc(n, sizeof(entry_t *));   // all NULL
    h->n_buckets = n;
    h->size      = 0;
}
```

`calloc` zeros out the buckets so all start as `NULL`.

### Insert

```c
int hmap_put(hmap_t *h, const char *key, int val) {
    unsigned int idx = fnv1a(key) % h->n_buckets;

    // (1) check if key already exists; update if so
    for (entry_t *e = h->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            e->val = val;
            return 0;
        }
    }

    // (2) prepend new entry to bucket's list
    entry_t *e = malloc(sizeof(*e));
    if (!e) return -1;
    e->key  = strdup(key);
    e->val  = val;
    e->next = h->buckets[idx];   // point to old head
    h->buckets[idx] = e;          // become new head
    h->size++;

    // (3) maybe resize if too full
    if (h->size > h->n_buckets * 3 / 4) hmap_resize(h, h->n_buckets * 2);
    return 0;
}
```

(1) **Walk the chain** to see if key exists. If yes, update value and return.
(2) Otherwise, **prepend** new entry to the chain (cheap — O(1) prepend, see DSA-04).
(3) If the table is **75% full**, **double** the bucket count and rehash all entries.

### Look-up

```c
int hmap_get(hmap_t *h, const char *key, int *out) {
    unsigned int idx = fnv1a(key) % h->n_buckets;
    for (entry_t *e = h->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            *out = e->val;
            return 0;
        }
    }
    return -1;     // not found
}
```

Compute hash → walk the chain → return value (or -1 if missing).

### Trace: get("dave") on our example table

1. `fnv1a("dave")` = some big number; `% 4` = 0.
2. `e = buckets[0]` → entry "bob". `strcmp("bob", "dave")` ≠ 0. e = e->next.
3. `e` → entry "dave". `strcmp("dave", "dave")` == 0 → return val.

Two strcmps, two pointer follows. **O(1)** on average (chain length is short).

## Why resize?

The "**load factor**" is `size / n_buckets` — average chain length.

- Load factor 0.5: most buckets have 0 or 1 entry. Lookups are essentially O(1).
- Load factor 1.0: chains average length 1. Still mostly O(1).
- Load factor 5.0: chains average length 5. Lookups are 5 strcmps. Becoming O(n).

To keep performance, **resize** when the load factor gets too high — typically when it crosses **0.75**.

Resize:
1. Allocate a new buckets array (e.g., 2× the size).
2. Walk every entry in the old buckets.
3. Re-hash each into the new table (the bucket index changes because `n_buckets` changed!).
4. Free the old buckets.

This is **O(n)** but happens rarely. Amortized cost per insert: still O(1).

## Why hash tables fail (worst case)

If all your keys hash to the same bucket, you've got one giant chain. Lookups become **O(n)**.

How could that happen?
- **Bad hash function:** every key produces the same hash.
- **Adversarial input:** an attacker chooses keys that hash to the same bucket (a denial-of-service attack on web servers).

Real systems use **randomized hashes** (SipHash with a per-boot random seed) so attackers can't predict bucket placement. The Linux kernel uses SipHash for hash tables that face untrusted input.

## Hash tables vs other structures

| | Hash table | BST | Sorted array |
|---|---|---|---|
| Lookup (avg) | **O(1)** | O(log n) | O(log n) |
| Lookup (worst) | O(n) | O(log n) (balanced) | O(log n) |
| Sorted iteration | impossible (random order) | **O(n) sorted** | O(n) sorted |
| Range query "all keys in [a,b]" | impossible | O(log n + k) | O(log n + k) |
| Memory | bucket array + entries | 2 ptrs + value per node | just the values |

**Use hash table** for "raw fast key→value lookup."
**Use BST or sorted array** when you need ordered iteration or range queries.

## Picture of resize (4 → 8 buckets)

Before resize (size=3, cap=4, load=0.75):

```
   buckets:
   0 ──▶ ["bob",8]  ──▶ ["dave",12] ──▶ NULL
   1 ──▶ NULL
   2 ──▶ NULL
   3 ──▶ ["alice",11] ──▶ NULL
```

We compute new bucket for each entry under cap=8:

| key | hash | new bucket = hash % 8 |
|---|---|---|
| "bob" | 8 | 0 |
| "dave" | 12 | 4 |
| "alice" | 11 | 3 |

After resize:

```
   buckets:
   0 ──▶ ["bob",8] ──▶ NULL
   1 ──▶ NULL
   2 ──▶ NULL
   3 ──▶ ["alice",11] ──▶ NULL
   4 ──▶ ["dave",12] ──▶ NULL
   5 ──▶ NULL
   6 ──▶ NULL
   7 ──▶ NULL
```

"bob" and "dave" used to share bucket 0; with twice the buckets, they spread out. New load = 3/8 ≈ 0.4. Plenty of room until next resize.

## Common application: the LRU cache (preview)

A common interview question: implement a cache that:
- maps key → value (fast, O(1)),
- evicts the **least-recently-used** key when full.

Solution: hash table + doubly linked list.

- Hash table: key → pointer to list node.
- Doubly linked list: kept in **most-recently-used → least-recently-used** order.

To `get(key)`: hash lookup → move that node to front of list.
To `put(key, val)`: insert new node at front; if at capacity, drop the last node.

Both operations: O(1).

Used in: every CPU cache, every web cache, the kernel's page cache (with twists). We trace the full implementation in DSA-26.

## Common bugs

### Bug 1: bad hash function

```c
unsigned int bad_hash(const char *s) {
    return s[0];           // hash by first char only — terrible!
}
```

All "apple", "ant", "axe" → hash 'a' = 97. They all collide. Lookup degrades to O(n).

### Bug 2: forgetting to compare keys

```c
// WRONG — only checks hash, not key
for (entry_t *e = buckets[idx]; e; e = e->next)
    return e->val;          // wrong! returns first entry, even if keys differ
```

Always `strcmp` (or memcmp, etc.) the actual key on lookup. Hash equality doesn't mean key equality.

### Bug 3: modifying a key after insertion

```c
char *k = strdup("hello");
hmap_put(h, k, 42);
strcpy(k, "world");           // mutating the key!
hmap_get(h, "world", ...);    // looks in a different bucket — won't find it
```

Hash keys must be **effectively immutable** while in the table.

### Bug 4: forgetting to resize

A growing hash table without resize gets slower and slower. Always have a resize policy.

## Linux kernel hash tables (preview)

The kernel has its own hash-table macros — read `include/linux/hashtable.h`. They use **embedded `hlist_node`** (similar to `list_head` from DSA-04).

The genius:
- No `malloc` per entry — caller embeds `hlist_node` in their own struct.
- No type erasure — `container_of` recovers the typed struct.
- Fast: written for cache locality.

Once you've understood the array+linked-list version above, the kernel version reads naturally.

## Recap

1. Hash table = **bucket array** + **hash function** that maps key → bucket.
2. **Average lookup is O(1).** Worst case O(n) (all collisions in one bucket).
3. Two collision strategies: **chaining** (linked list per bucket) or **open addressing** (probe).
4. Resize when load factor > 0.75 (or similar). Amortized cost stays O(1).
5. **Don't use** for ordered iteration or range queries.
6. The hash function is critical. Bad hash = lots of collisions = slow.

## Self-check (in your head)

1. With `n_buckets = 8` and `hash("foo") = 19`, which bucket does "foo" go to?

2. You insert 10 keys into a hash table with 4 buckets. What's the average chain length? What about with 100 buckets?

3. Two keys hash to the same number. What does the chained hash table do?

4. Why is comparing two keys via `==` (or just hash equality) a bug?

5. A hash table is at load factor 0.9 and getting slower. What do you do?

---

**Answers:**

1. `19 % 8 = 3`. **Bucket 3.**

2. With 4 buckets: 10 / 4 = **2.5 average chain length**. With 100 buckets: 10 / 100 = **0.1** — most buckets empty, no chains needed.

3. Both end up in the **same bucket**, in the bucket's chain (linked list). Lookup walks the chain and uses key comparison to distinguish them.

4. Different keys can have the same hash (collision). Same hash ≠ same key. Always compare the actual key to confirm.

5. **Resize.** Allocate a bigger bucket array (typically double); rehash every entry into the new buckets. Load factor drops back near 0.4–0.5; lookups speed up.

If 4/5 you understand hash tables. 5/5, move on.

Next → [DSA-07 — Trees & BSTs](DSA-07-trees.md).
