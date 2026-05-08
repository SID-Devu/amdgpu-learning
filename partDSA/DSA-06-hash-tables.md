# DSA 6 — Hash tables

> The most-used data structure in computer science. Average O(1) lookup. Master it.

## The idea

Given a key `k`, compute an integer `h = hash(k)`. Use `h % n_buckets` to pick a bucket. Store the value there.

When two keys hit the same bucket → **collision**. We resolve collisions in two main ways:
- **Chaining** — each bucket holds a linked list (or list of pairs) of entries that hashed there.
- **Open addressing** — if the slot is taken, probe forward to find another.

## Hash functions

A good hash:
- distributes keys uniformly across buckets;
- is fast to compute;
- avoids correlation with input patterns.

For integers, `multiply by a large prime, mod n`:
```c
unsigned int hash_int(unsigned int x) { return x * 2654435769u; }   // Fibonacci hash
```

For strings, **FNV-1a** is simple and decent:
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

For production: **xxHash**, **CityHash**, **SipHash** (cryptographic — used in Linux to prevent hash-flooding DoS).

Cryptographic hashes (SHA-256, etc.) are overkill for hash tables; far too slow. Use them for security, not for table lookups.

## Chaining hash table — full implementation

```c
typedef struct entry {
    char *key;
    int val;
    struct entry *next;
} entry_t;

typedef struct {
    entry_t **buckets;
    size_t n_buckets;
    size_t size;
} hmap_t;

void hmap_init(hmap_t *h, size_t n) {
    h->buckets = calloc(n, sizeof(entry_t *));
    h->n_buckets = n;
    h->size = 0;
}

int hmap_put(hmap_t *h, const char *key, int val) {
    unsigned int idx = fnv1a(key) % h->n_buckets;
    for (entry_t *e = h->buckets[idx]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) { e->val = val; return 0; }
    }
    entry_t *e = malloc(sizeof(*e));
    if (!e) return -1;
    e->key = strdup(key);
    e->val = val;
    e->next = h->buckets[idx];
    h->buckets[idx] = e;
    h->size++;
    if (h->size > h->n_buckets * 3 / 4) hmap_resize(h, h->n_buckets * 2);
    return 0;
}

int hmap_get(hmap_t *h, const char *key, int *out) {
    unsigned int idx = fnv1a(key) % h->n_buckets;
    for (entry_t *e = h->buckets[idx]; e; e = e->next)
        if (strcmp(e->key, key) == 0) { *out = e->val; return 0; }
    return -1;
}

int hmap_del(hmap_t *h, const char *key) {
    unsigned int idx = fnv1a(key) % h->n_buckets;
    entry_t **pp = &h->buckets[idx];
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            entry_t *e = *pp;
            *pp = e->next;
            free(e->key); free(e);
            h->size--;
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}
```

The `hmap_resize` is left as exercise: allocate new bucket array, walk all old entries, rehash into new buckets, free old.

### Why resize?
Load factor (size / n_buckets) > ~0.75 → average chain length grows → lookups slow down. Doubling and rehashing keeps average chain ~O(1).

## Open-addressing hash table

No allocations per entry — store directly in the bucket array. When collision, **probe**:

- **Linear probing**: try `idx, idx+1, idx+2, ...` (mod n).
- **Quadratic probing**: try `idx + 1², idx + 2², ...`.
- **Double hashing**: try `idx + i*hash2(key)`.

Each slot has a state: empty, occupied, deleted (tombstone).

```c
typedef struct { char *key; int val; int state; } slot_t;
// state: 0 = empty, 1 = occupied, 2 = tombstone
```

Pros: no per-entry malloc, very cache-friendly.
Cons: tombstones accumulate; deletions are messy; clustering with linear probing.

The Linux kernel's `hashtable.h` is **chained** with embedded `hlist_node` — no malloc on insert (the user provides the entry).

## Hash collisions and security

Earlier kernels used a fixed-seed hash. An attacker could craft keys all hashing to the same bucket → O(n²) blowup → DoS. Linux now uses `SipHash` with a random per-boot key for hashes that face untrusted input. **Lesson**: if your hash table will see attacker-controlled keys, use a randomized/keyed hash.

## When hash tables are wrong

- **Sorted iteration needed?** Use a tree.
- **Range queries (`give me all keys in [a,b]`)?** Use a tree.
- **Memory tight, key set small?** Use a sorted array + binary search.
- **Worst-case latency critical?** Hash tables can spike. Tree is more predictable.

## Linux `hashtable.h`

```c
#include <linux/hashtable.h>

DEFINE_HASHTABLE(my_hash, 8);   // 1 << 8 = 256 buckets

struct foo {
    u32 key;
    int val;
    struct hlist_node node;     // embed
};

void insert(struct foo *f) { hash_add(my_hash, &f->node, f->key); }

struct foo *find(u32 k) {
    struct foo *f;
    hash_for_each_possible(my_hash, f, node, k)
        if (f->key == k) return f;
    return NULL;
}
```

Beautifully zero-allocation. Read `include/linux/hashtable.h`.

## Hash tables in real life

- compiler symbol tables;
- DNS caches;
- Linux's dentry cache (`fs/dcache.c`);
- the file descriptor table for an ungodly number of open fds (in some contexts);
- amdgpu's BO lookup tables;
- Mesa's shader caches.

## Bloom filter (preview)

If you only need "definitely no" or "maybe yes", a **bloom filter** uses `O(n)` bits, no false negatives, controllable false positives. Used for: cache miss avoidance, web crawl dedup, blockchain. We'll do this in DSA-26.

## Common bugs

1. Bad hash function → many collisions → lookups slow.
2. Forgetting to compare keys (only checking hash) → false matches because two different keys hash the same.
3. Modifying a key after insertion → it's now in the wrong bucket. **Hash keys must be effectively immutable.**
4. Forgetting to handle resize gracefully.
5. Memory leak: deleting an entry without freeing its key/value.

## Try it

1. Implement chaining hash map from the chapter, including resize-on-load. Test with 1 million inserts; print the largest chain length (should be small).
2. Implement open-addressing hash map with linear probing. Compare insert/lookup speed to chaining.
3. Implement an "anagram grouping" function: given a list of strings, group those that are anagrams of each other. Use sorted-letters as the hash map key.
4. Implement two-sum in O(n): given `arr` and target `t`, return indices `i, j` where `arr[i]+arr[j] == t`.
5. Read `include/linux/hashtable.h`. Find: how does it pick buckets? What's the macro that iterates a single bucket?
6. Bonus: implement a **LRU cache** with `O(1)` get/put using a hash map plus a doubly linked list. (This is a top-asked interview question. Solution in DSA-26.)

## Read deeper

- *Hopscotch hashing* and *Robin Hood hashing* — modern open-addressing variants used by Rust's HashMap, etc.
- "Hashing the bytestream" — the SipHash paper (short, very readable).
- `lib/hashtable.c` and `lib/rhashtable.c` (resizable, RCU-safe) — the kernel's hash table heavyweights.

Next → [Binary trees & BSTs](DSA-07-trees.md).
