# DSA 13 — Tries

> A tree where keys are strings (or other sequences), branching at each character. The data structure behind autocomplete, IP routing, dictionaries, and bioinformatics.

## The structure

Each node has up to (alphabet size) children, plus a flag for "end of word."

```c
typedef struct trie_node {
    struct trie_node *children[26];   // for lowercase a-z
    int is_word;
} trie_t;

trie_t *trie_new() { return calloc(1, sizeof(trie_t)); }

void trie_insert(trie_t *root, const char *s) {
    trie_t *cur = root;
    for (; *s; s++) {
        int idx = *s - 'a';
        if (!cur->children[idx]) cur->children[idx] = trie_new();
        cur = cur->children[idx];
    }
    cur->is_word = 1;
}

int trie_find(trie_t *root, const char *s) {
    trie_t *cur = root;
    for (; *s; s++) {
        int idx = *s - 'a';
        if (!cur->children[idx]) return 0;
        cur = cur->children[idx];
    }
    return cur->is_word;
}

int trie_starts_with(trie_t *root, const char *prefix) {
    trie_t *cur = root;
    for (; *prefix; prefix++) {
        int idx = *prefix - 'a';
        if (!cur->children[idx]) return 0;
        cur = cur->children[idx];
    }
    return 1;
}
```

### Costs
- Insert/find/starts-with: **O(m)** where m = key length. Independent of n words!
- Memory: up to alphabet × #nodes. For ASCII (256), this can balloon.

## Why a trie beats a hash table

- **Prefix queries** (`autocomplete`) — hash tables can't do this; trie is natural.
- **Sorted iteration** by walking the trie alphabetically.
- **No collisions** — exact path = exact key. Worst case == average.

## Why a trie loses

- **Memory overhead**: each node has 26 pointers (or 256). Sparse tries waste memory.
- **Cache unfriendly** vs. flat hash table.
- **Complexity** of code.

For dense-character alphabets, mitigate with:
- a **HashMap** in each node instead of an array (smaller, slightly slower);
- **compressed tries** (skip-nodes): collapse single-child chains.

## Compressed trie / Patricia trie / Radix tree

When a path has only single children, collapse them into one edge labeled with the substring. Saves memory dramatically for sparse data.

The **Linux radix tree** (`include/linux/radix-tree.h`, gradually replaced by `XArray`) is exactly this. Used to map page indices to `struct page`. The radix tree's keys are integers, not strings, but the principle is identical: branch on chunks of bits.

`XArray` is the modern replacement: a clearer API, easier multi-threaded use, RCU-safe lookups.

## Real applications

### Autocomplete
After typing "co", traverse to the "co" node. DFS from there, collect all words. Optionally rank by frequency or recency.

### IP routing
A router maps an IP destination to the **longest matching prefix** in its routing table. Bit-trie of IP addresses; longest-prefix-match in O(32) for IPv4, O(128) for IPv6.

Implementations:
- **Patricia trie** (BSD).
- **DXR**, **LC-tries**, **multibit tries** for hardware acceleration.

### Dictionary / spellcheck
"Is this a word?" `trie_find` in O(m). Plus near-match suggestions via DFS with edit-distance budget.

### Bioinformatics
Suffix tries for genome matching. Built into suffix arrays/trees for sublinear pattern search (DSA-25).

### Aho-Corasick
A trie + failure links. Searches multiple patterns in one pass through text. Used in `grep -F` (with multiple patterns), virus scanners, network intrusion detection.

## In the kernel

- `lib/radix-tree.c` — was widely used (page cache, IDR).
- `lib/xarray.c` — modern replacement. Now powers page cache mapping.
- `net/ipv4/fib_trie.c` — FIB (forwarding information base) uses an LC-trie.

Read `lib/xarray.c` once you've finished the kernel chapters — it's a beautiful piece of code.

## Common bugs

1. **Off-the-end alphabet index**: `'A' - 'a'` is negative; non-ASCII bytes blow up the index.
2. **Shared word + prefix**: forgetting `is_word` and treating any visited node as a word.
3. **Memory leak**: trie cleanup needs post-order recursion.
4. **Collisions in compressed tries**: forgetting to split an edge when a partial match occurs.

## Try it

1. Implement the basic trie above. Insert 10,000 dictionary words; test lookups.
2. Build autocomplete: given prefix, return all completions.
3. Implement word-search-II (Leetcode 212): given a board of letters and a word list, find which words are present (cells contiguous, no reuse). Trie + DFS.
4. Implement an IP-prefix matcher: insert (prefix, length, value) tuples; longest-prefix-match an IP.
5. Bonus: implement Aho-Corasick. Search a text for ~10 patterns simultaneously. (~150 lines if you're patient.)

## Read deeper

- `lib/xarray.c` and `Documentation/core-api/xarray.rst`.
- Knuth *TAOCP* vol. 3 — searching.
- "Algorithms on Strings, Trees, and Sequences" (Gusfield) — the bible for string-on-trie work.

Next → [Sorting algorithms](DSA-14-sorting.md).
