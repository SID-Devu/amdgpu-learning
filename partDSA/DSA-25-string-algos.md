# DSA 25 — String algorithms: KMP, Rabin-Karp, Z, suffix arrays

> Sub-quadratic pattern matching. The substrate beneath grep, search engines, bioinformatics.

## The problem

Given **text** of length n and **pattern** of length m, find all occurrences of pattern in text.

- Naive: at each text position, compare m characters → O(n·m).
- KMP / Z / Boyer-Moore: O(n + m).
- Rabin-Karp: O(n + m) average, O(n·m) worst.
- Suffix arrays / suffix trees: massively useful for repeated queries.

## KMP (Knuth-Morris-Pratt)

Idea: when a partial match fails, **don't go back in the text**. Use a precomputed "failure function" of the pattern that tells you how much of the pattern is also a prefix.

### Prefix function ("failure function")

For pattern `P`, compute `pi[i]` = length of the longest proper prefix of `P[0..i]` that's also a suffix.

```c
void build_pi(const char *p, int m, int *pi) {
    pi[0] = 0;
    int k = 0;
    for (int i = 1; i < m; i++) {
        while (k > 0 && p[k] != p[i]) k = pi[k-1];
        if (p[k] == p[i]) k++;
        pi[i] = k;
    }
}
```

### Matching

```c
void kmp(const char *t, int n, const char *p, int m) {
    int pi[m]; build_pi(p, m, pi);
    int k = 0;
    for (int i = 0; i < n; i++) {
        while (k > 0 && p[k] != t[i]) k = pi[k-1];
        if (p[k] == t[i]) k++;
        if (k == m) {
            printf("match at %d\n", i - m + 1);
            k = pi[k-1];
        }
    }
}
```

O(n + m). One of the cleanest non-trivial algorithms in the book — and one of the most common interview questions for senior roles.

## Rabin-Karp

Rolling hash. Compute hash of pattern; slide a window over text computing hash of each m-substring. When hashes match, verify with direct comparison.

```c
unsigned long long mod = 1000000007;
unsigned long long base = 31;

unsigned long long hash_str(const char *s, int len) {
    unsigned long long h = 0;
    for (int i = 0; i < len; i++) h = (h * base + s[i]) % mod;
    return h;
}
```

To slide the window, you need `base^(m-1) mod p` precomputed; on each step:
```
new = (old - s[i] * base^(m-1)) * base + s[i+m]
```
all mod p.

- Average O(n + m).
- Worst O(n·m) (adversarial input causing many false hash matches).
- Used in: plagiarism detection, fingerprinting (rsync delta), bytecode dedup.

Always **double-check** with direct compare on hash match — hash collisions are real.

## Z-function

For string `s`, `Z[i]` = length of longest substring starting at `i` that matches a prefix of `s`.

Compute in O(n) with a clever two-pointer trick. Used in:
- pattern matching (concat pattern + sentinel + text → look for `Z[i] == m`),
- detecting repeats,
- string compression.

## Boyer-Moore

Match from the **right** of the pattern. Bad-character + good-suffix heuristics let you skip large chunks. **Sublinear in practice** (less than n comparisons on natural text). Used in `grep`, `strstr` on glibc.

Implementation is involved (~100 lines); read the wiki article for the rules.

## Aho-Corasick

For matching **multiple patterns** simultaneously. Build a trie of patterns; add "failure links" (like KMP's pi function generalized). Linear time.

Used in: `fgrep`, `snort` (intrusion detection), virus scanners, lexical analyzers.

## Suffix arrays

A sorted array of all **suffixes** of a string. Fundamental for many string operations.

For "abc":
- suffixes: "abc", "bc", "c"
- sorted: "abc", "bc", "c"
- suffix array: [0, 1, 2]

Construction in O(n log n) (or O(n) with SA-IS). Plus the **LCP array** (longest common prefix between consecutive sorted suffixes) gives O(n) for many problems.

Use cases:
- find all occurrences of a pattern in O(m log n + k);
- count distinct substrings;
- longest repeated substring;
- bioinformatics: BWT, FM-index used in bwa for read alignment.

## Suffix tree

A trie of all suffixes, compressed (Ukkonen's algorithm builds it in O(n)). Strictly more expressive than suffix array; uses more memory; harder to implement. In modern practice, **suffix arrays + LCP** are usually preferred.

## Edit distance algorithms

- **Levenshtein** distance — DP we did in DSA-17, O(n*m).
- **Wagner-Fischer** — same algorithm, classic name.
- **Bit-parallel** for short patterns — O(n·m/w) where w = word width.

Used in: spell checkers, fuzzy search, DNA alignment.

## Compression algorithms (briefly)

Strings overlap heavily with compression:
- **Run-length encoding** — consecutive duplicates.
- **Huffman coding** — optimal prefix code.
- **LZ77 / LZ78** — repeat references; basis of zlib/gzip.
- **Burrows-Wheeler Transform** — used in bzip2 and bwa (bioinformatics aligner).

## In the real world

- **grep / ripgrep** — Boyer-Moore family for fixed strings; Aho-Corasick for `-F` with multiple patterns; regex engines for the rest.
- **Code search** (GitHub, sourcegraph) — suffix arrays / FM-index for fast on-disk search.
- **AV / IDS** (ClamAV, Snort) — Aho-Corasick.
- **DNA aligners** (BWA, Bowtie) — FM-index.
- **rsync** — rolling hash for delta sync.
- **Compilers** — KMP-style for tokenization in some lexers; suffix tries for token dedup.

## Common bugs

1. **Hash collisions** in Rabin-Karp without verification.
2. **Off-by-one in pi function** during recursion (`pi[k-1]` vs `pi[k]`).
3. **Pattern containing the sentinel** in Z-function tricks.
4. **Index types**: use `size_t` or `int64_t` for big strings.
5. **Memory blow-up**: suffix tree for a 100 MB file is huge — use suffix array.

## Try it (must-do)

1. Implement KMP. Verify on a few test cases.
2. Implement Rabin-Karp. Confirm hash-collision verification step.
3. Implement Z-function. Use it for pattern matching by `concat(p, '#', t)` trick.
4. Build a suffix array via simple O(n² log n) sort. Run on a small string and verify by hand.
5. Build LCP array using Kasai's algorithm. Use it to find longest repeated substring.
6. Bonus: implement Aho-Corasick on a list of 10 patterns and search a text.

## Read deeper

- "Algorithms on Strings, Trees, and Sequences" by Dan Gusfield — the bible.
- *Competitive Programmer's Handbook* — clean intros to KMP, Z, suffix array.
- "BWA: a fast aligner using the Burrows-Wheeler Transform" — Heng Li et al.

Next → [System-design DSA: LRU cache, bloom filter, skip list, consistent hashing](DSA-26-system-design-ds.md).
