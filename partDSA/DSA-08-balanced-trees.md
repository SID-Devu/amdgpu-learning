# DSA 8 — Self-balancing trees: AVL & Red-Black

> A plain BST can degrade to O(n). Self-balancing trees keep height **Θ(log n)**, always. The Linux kernel's `rb_tree` is one of them.

## The problem balanced trees solve

Insert `1, 2, 3, 4, 5` into a plain BST → a chain of right children, height 5. Lookup is now O(n).

Solution: after each insert/delete, **rotate** the tree to restore some height invariant.

## Rotations — the universal primitive

A **rotation** changes the structure without breaking the BST invariant. Two flavors: **left** and **right**.

```
Right rotate at y:
       y                    x
      / \                  / \
     x   c       →        a   y
    / \                      / \
   a   b                    b   c
```

```c
tnode_t *rotate_right(tnode_t *y) {
    tnode_t *x = y->left;
    y->left = x->right;
    x->right = y;
    return x;   // new root of this subtree
}

tnode_t *rotate_left(tnode_t *x) {
    tnode_t *y = x->right;
    x->right = y->left;
    y->left = x;
    return y;
}
```

Constant time. Preserves BST ordering. The whole rebalancing trick is "after a possibly-bad insert, do at most a constant number of rotations to fix the invariant."

## AVL trees

Invariant: for every node, **|height(left) − height(right)| ≤ 1**.

Each node stores its height (or balance factor `+1, 0, -1`).

### Insert
1. Insert as in a normal BST.
2. Walk back up; recompute height; if unbalanced, rotate (4 cases: LL, LR, RR, RL).

```c
typedef struct anode {
    int val, height;
    struct anode *left, *right;
} anode_t;

int h(anode_t *n) { return n ? n->height : 0; }
int bf(anode_t *n) { return h(n->left) - h(n->right); }
void update_h(anode_t *n) { n->height = 1 + (h(n->left) > h(n->right) ? h(n->left) : h(n->right)); }

anode_t *avl_insert(anode_t *r, int v) {
    if (!r) { anode_t *n = calloc(1, sizeof(*n)); n->val = v; n->height = 1; return n; }
    if (v < r->val) r->left = avl_insert(r->left, v);
    else if (v > r->val) r->right = avl_insert(r->right, v);
    else return r;

    update_h(r);
    int b = bf(r);
    // LL
    if (b > 1 && v < r->left->val) return rotate_right(r);
    // RR
    if (b < -1 && v > r->right->val) return rotate_left(r);
    // LR
    if (b > 1 && v > r->left->val) { r->left = rotate_left(r->left); return rotate_right(r); }
    // RL
    if (b < -1 && v < r->right->val) { r->right = rotate_right(r->right); return rotate_left(r); }
    return r;
}
```

(Note: rotate_left/rotate_right need to also call update_h; left as exercise.)

### Performance
- Strict balance → **fastest lookups** of any common balanced tree.
- More rotations on insert/delete than red-black trees. So **lookups faster, modifications slower**.
- Used when reads dominate writes.

## Red-Black trees

Invariant: every node is **red** or **black**, plus 4 rules:
1. Root is black.
2. Red nodes have black children (no two reds in a row).
3. Every path from root to a NULL pointer passes through the same number of black nodes (**black-height invariant**).
4. (NULL pointers count as black.)

These rules together guarantee **height ≤ 2 log₂(n+1)** — O(log n).

Looser balance than AVL → fewer rotations on insert/delete (at most 2 rotations + recoloring per insert; at most 3 rotations per delete). So **modifications faster, lookups slightly slower**.

This is why Linux uses red-black trees: kernel data structures often see frequent inserts/deletes (e.g., adding/removing VMAs, scheduler runqueue, GPU memory ranges).

### Code? Don't write it.

Real RB-tree code is dozens of cases (insert fixup vs delete fixup, with sibling-color and uncle-color subcases). It's correct, but tedious. **Read** Linux's `lib/rbtree.c`; don't reimplement unless you must.

The Linux RB-tree API is **augmented** — you can attach a callback that gets called whenever a subtree changes, used to maintain extra info like `interval_tree`'s "max endpoint in subtree" for fast interval queries.

## Linux `rb_tree` — practical usage

`include/linux/rbtree.h`:

```c
struct rb_node {
    unsigned long  __rb_parent_color;
    struct rb_node *rb_right, *rb_left;
};

struct rb_root { struct rb_node *rb_node; };

#define RB_ROOT (struct rb_root) { NULL, }
```

Just like `list_head`, the link is **embedded** in your struct via `rb_node`, and `rb_entry` (which is `container_of`) recovers the containing struct.

You provide your own insert function (the kernel can't compare your keys generically):

```c
struct gpu_bo {
    u64 vaddr;
    struct rb_node node;
    /* ... */
};

void bo_insert(struct rb_root *root, struct gpu_bo *new) {
    struct rb_node **p = &root->rb_node, *parent = NULL;
    while (*p) {
        struct gpu_bo *e = rb_entry(*p, struct gpu_bo, node);
        parent = *p;
        if (new->vaddr < e->vaddr) p = &(*p)->rb_left;
        else if (new->vaddr > e->vaddr) p = &(*p)->rb_right;
        else { /* duplicate handling */ return; }
    }
    rb_link_node(&new->node, parent, p);
    rb_insert_color(&new->node, root);
}

struct gpu_bo *bo_find(struct rb_root *root, u64 addr) {
    struct rb_node *n = root->rb_node;
    while (n) {
        struct gpu_bo *e = rb_entry(n, struct gpu_bo, node);
        if (addr < e->vaddr) n = n->rb_left;
        else if (addr > e->vaddr) n = n->rb_right;
        else return e;
    }
    return NULL;
}
```

This pattern is **everywhere in amdgpu** — for tracking VAs, PT entries, BO ranges. Memorize it.

## Other balanced trees you should know exist

- **Splay trees** — recently-accessed elements bubble to the root. Not balanced in the classical sense, but amortized O(log n). Used in some kernels (BSD), some compilers.
- **Treaps** — BST + heap-on-priorities (priorities random). Probabilistically balanced. Simple code.
- **Skip lists** — randomized linked-list-of-linked-lists. Lock-friendly. Used in concurrent contexts (e.g., LevelDB, Java's `ConcurrentSkipListMap`).
- **B-trees / B+trees** — many children per node. Disk- and cache-friendly. Filesystems (xfs, btrfs, ext4 dirs) and DBs use them. We do these next.

## When to use what (cheat sheet)

| Need | Use |
|---|---|
| Fastest get / put unordered | hash table |
| Ordered iteration | BST (red-black or AVL) |
| Mostly reads | AVL |
| Mostly writes | Red-Black |
| On-disk / huge dataset | B-tree / B+tree |
| Lock-free ordered | Skip list |
| Range queries with intervals | interval tree (RB-augmented) |

## Common bugs (you will hit them)

1. After rotating, **forgetting to update height/color** before rotating again.
2. Updating height **before** doing the rotation, getting wrong values.
3. Mismanaging the **parent pointer** in RB trees (Linux stores parent + color packed into one word — read carefully).
4. Augmenting (adding extra info per node) without telling the rebalance code → bugs after every rotation.

## Try it

1. Implement AVL insert and the four rotation cases. Test by inserting 1..1000 sorted and verifying height ≈ log₂(1000).
2. Implement AVL delete (similar idea — rebalance up the path).
3. Use the Linux RB-tree pattern in a userspace test (you can copy `lib/rbtree.c` + `include/linux/rbtree.h` to userspace; many hobby projects do).
4. Build an **interval tree**: tree of intervals, augmented with "max endpoint in subtree." Support insert, delete, and "find any interval overlapping [a,b]" in O(log n).
5. Read `lib/rbtree.c`. Identify `__rb_insert`, `__rb_erase_color`. Don't try to memorize — appreciate the structure.

## Read deeper

- CLRS chapter 13 (Red-Black Trees) and chapter 14 (Augmenting Data Structures).
- Documentation: `Documentation/core-api/rbtree.rst`.
- LWN article: "An introduction to the rbtree library" — short and excellent.
- For B-trees, Hellerstein/Stonebraker's "Anatomy of a Database System."

Next → [B-trees and B+trees](DSA-09-btrees.md).
