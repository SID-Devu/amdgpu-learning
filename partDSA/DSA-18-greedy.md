# DSA 18 — Greedy algorithms

> "Make the best local choice; trust it gives the global best." Sometimes works; often doesn't. The skill is knowing when.

## What "greedy" means

At each step, pick the choice that looks best **right now**, without backtracking or considering future consequences.

It's the **simplest** algorithmic approach. When it works, it's typically O(n log n) (after sorting) or O(n).

## When greedy works (and you can prove it)

A greedy algorithm is correct when the problem has:

1. **Greedy choice property** — making the locally optimal choice leads to a globally optimal solution.
2. **Optimal substructure** — after the greedy choice, the remaining problem is the same kind of problem (smaller).

Often you prove correctness by:
- **Exchange argument**: assume some other optimal solution; swap its first choice for the greedy one; show no worse.
- **Inductive proof**: greedy choice is consistent with some optimal.

## Classic greedy algorithms

### 1. Activity selection (interval scheduling)
"Pick max # of non-overlapping intervals."

Greedy: sort by **end time**; pick the earliest-ending; skip overlaps; repeat.

```c
int activity(interval *a, int n) {
    qsort(a, n, sizeof(interval), cmp_by_end);
    int count = 0, end = -1;
    for (int i = 0; i < n; i++)
        if (a[i].start >= end) { count++; end = a[i].end; }
    return count;
}
```
O(n log n). Optimal — provable by exchange.

### 2. Huffman coding
Optimal prefix code for compression.

Greedy: repeatedly merge the two least-frequent symbols (priority queue) until one tree remains.

O(n log n). Used in: gzip, zip (DEFLATE), JPEG, MP3.

### 3. Minimum Spanning Tree

#### Kruskal's
Sort edges by weight; add to result if it doesn't form a cycle (use union-find — DSA-23).

#### Prim's
Grow from a starting vertex; always add cheapest edge to an unvisited vertex (priority queue).

Both O(E log V). Used in: network design, clustering, **chip layout**.

### 4. Dijkstra's shortest path
DSA-12. Greedy: extract closest vertex, relax. Optimal because of non-negative weights.

### 5. Fractional knapsack
Items with value and weight; can take fractions. Sort by value/weight ratio; take greedily until full.
**(Note: 0/1 knapsack is NOT greedy — needs DP.)**

### 6. Coin change with canonical denominations
US coins (1, 5, 10, 25, 100): greedy works (always take the largest).
Arbitrary denominations: greedy can fail. E.g., coins {1, 3, 4}, target 6: greedy gives 4+1+1=3 coins; optimal is 3+3=2 coins. Use DP.

### 7. Job scheduling with deadlines
"n jobs, each with deadline and profit. Each takes 1 unit. Maximize profit."

Greedy: sort by profit (desc); for each job, place in latest free slot ≤ deadline. Use union-find for fast slot finding.

### 8. Gas station problem
"Cars at stations, each station has fuel `a[i]`, distance to next is `b[i]`. From which station can you complete a circle?"

Greedy with single pass; classic interview problem.

### 9. Minimum number of platforms
"Trains arrive and depart; min number of platforms?"

Sort arrivals and departures; sweep and count overlapping intervals.

### 10. Reordering for minimum cost
Many "rearrange so X is minimized" problems are greedy after a sort.

## When greedy fails (and you must use DP)

- 0/1 Knapsack — picking the highest value/weight ratio first can leave you stuck.
- Coin change with arbitrary denominations.
- Edit distance, LCS.
- Longest path (NP-hard in general).

**Rule of thumb**: if the answer requires considering dependencies between distant choices, greedy fails. DP saves you.

## Greedy mindset for problem solving

1. Identify the **cost function** to minimize or **profit function** to maximize.
2. Ask: "what's the locally best move?"
3. Try a small example. Does greedy give the right answer?
4. Try an adversarial example. Does it still work?
5. If not, look for a DP framing.

## Real-world greedy

- **Compiler register allocation** — graph coloring is NP-hard, but linear-scan greedy works in production compilers (LLVM has both greedy and basic-PBQP allocators).
- **Schedulers** — pick the next task by priority. Strict greedy (priority queue). Works because we accept "not provably optimal."
- **Cache eviction** — LRU is greedy ("evict the one I haven't used longest"). Belady's optimal needs the future, which we don't have.
- **GPU work-stealing** — each thread greedily steals from longest queue. Heuristic, but practical.

## Common bugs

1. **Wrong sort key** — sorting by start time instead of end time in activity selection breaks correctness.
2. **Greedy where DP is required** — produces wrong answer; small testing may not catch it.
3. **Failing to break ties consistently** — flapping between solutions.
4. **Off-by-one in interval inclusion** — `>=` vs `>` for "starts after previous ends."

## Try it

1. Activity selection — implement and test.
2. Fractional knapsack — sort by value/weight, fill greedily. Compare to 0/1 knapsack on the same data.
3. Huffman code — build the tree; encode and decode a sample string. Verify length is reduced.
4. Min-platforms — implement and test on sample arrivals/departures.
5. Greedy coin change with `{1,5,10,25}` (US) and `{1,3,4}`. Show greedy succeeds for the first and fails for the second; solve the second with DP.
6. Bonus: Kruskal's MST with union-find (after DSA-23).

## Read deeper

- CLRS chapter 16 (Greedy Algorithms).
- "Greedy Algorithms" chapter in *Algorithm Design* by Kleinberg & Tardos — strong on proofs.
- "Linear-scan register allocation" Poletto & Sarkar (1999) — real-world greedy in compilers.

Next → [Divide & conquer](DSA-19-divide-conquer.md).
