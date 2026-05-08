# DSA 4 — Linked lists (single, double, circular)

> The Linux kernel uses linked lists in **thousands of places**. If you understand `list_head`, you understand half the kernel.

## What a linked list is

A list of nodes, each holding a value and a pointer to the next node.

```
[10|next] -> [20|next] -> [30|next] -> NULL
```

Trade-offs vs. arrays:

| | Array | Linked list |
|---|---|---|
| Random access `arr[i]` | O(1) | O(n) |
| Insert at front | O(n) | O(1) |
| Insert at end | O(1)* | O(1) (with tail ptr) |
| Memory | Contiguous, cache-friendly | Scattered, more cache misses |
| Resize | Realloc / double | Just allocate one node |

\* O(1) amortized for dynamic array.

**When to use a linked list**: when you do many insertions/deletions in the middle and you have direct pointers to nodes. **When NOT to use**: random access; tight inner loops where cache matters.

## Singly linked list — implement once, by hand

```c
typedef struct node {
    int val;
    struct node *next;
} node_t;

typedef struct {
    node_t *head;
    size_t size;
} list_t;

void list_init(list_t *l) { l->head = NULL; l->size = 0; }

int list_push_front(list_t *l, int v) {
    node_t *n = malloc(sizeof(*n));
    if (!n) return -1;
    n->val = v;
    n->next = l->head;
    l->head = n;
    l->size++;
    return 0;
}

int list_pop_front(list_t *l, int *out) {
    if (!l->head) return -1;
    node_t *n = l->head;
    *out = n->val;
    l->head = n->next;
    free(n);
    l->size--;
    return 0;
}

int list_contains(list_t *l, int v) {
    for (node_t *p = l->head; p; p = p->next)
        if (p->val == v) return 1;
    return 0;
}

void list_free(list_t *l) {
    node_t *p = l->head;
    while (p) {
        node_t *next = p->next;
        free(p);
        p = next;
    }
    l->head = NULL;
    l->size = 0;
}
```

That's the entire data structure. Type it from memory until you can in 5 minutes.

## Reverse a linked list (the famous interview question)

### Iterative — O(n), O(1) space
```c
node_t *reverse(node_t *head) {
    node_t *prev = NULL, *cur = head;
    while (cur) {
        node_t *next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    return prev;   // new head
}
```

### Recursive — O(n) time, O(n) space (stack)
```c
node_t *reverse_rec(node_t *head) {
    if (!head || !head->next) return head;
    node_t *new_head = reverse_rec(head->next);
    head->next->next = head;
    head->next = NULL;
    return new_head;
}
```

You should be able to write the iterative one without thinking. It comes up in **every** interview.

## Detect a cycle (Floyd's tortoise & hare)

If a `next` pointer eventually loops back, naive traversal infinite-loops. Detect with two pointers — slow steps 1, fast steps 2. If they ever meet, there's a cycle.

```c
int has_cycle(node_t *head) {
    node_t *slow = head, *fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return 1;
    }
    return 0;
}
```

O(n), O(1) space. Used in real-world checksums and parser correctness.

## Find the middle (one-pass)
Same trick — when fast hits end, slow is at middle.

## Find Nth-from-end (one-pass)
Two pointers, gap of N. When the lead hits NULL, the trailing pointer is at "nth from end."

These are the **two-pointer pattern** in disguise — see DSA-21.

## Doubly linked list

Each node has both `prev` and `next`.

```c
typedef struct dnode {
    int val;
    struct dnode *prev, *next;
} dnode_t;
```

Cost: extra pointer per node (more memory, worse cache).
Benefit: **O(1) deletion of a known node** (no need to scan for the predecessor).

This is exactly what the Linux kernel's `list_head` is, plus a clever trick we'll see now.

## The Linux kernel `list_head` (the most important DS in the kernel)

In `include/linux/list.h`:

```c
struct list_head {
    struct list_head *next, *prev;
};
```

That's it — just the **link**, no data. To make any struct listable:

```c
struct task {
    int pid;
    struct list_head node;        // embed the link
    ...
};
```

You build a list:
```c
LIST_HEAD(mylist);                       // statically defined
list_add(&task->node, &mylist);          // add at front
list_add_tail(&task->node, &mylist);     // add at back
list_del(&task->node);                   // remove
```

To iterate:
```c
struct task *t;
list_for_each_entry(t, &mylist, node) {
    pr_info("pid=%d\n", t->pid);
}
```

How does it know the offset of `node` inside `struct task`? Through the macro `container_of`:

```c
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
```

This goes from a `list_head *` to the containing `struct task *`. **One of the single most important macros in the kernel.** You will use it daily.

The genius: a generic, type-safe linked list with **zero memory overhead** beyond two pointers, and you can put a struct on **multiple lists at once** by embedding multiple `list_head` members.

```c
struct buffer {
    int id;
    struct list_head all;        // on the global list of all buffers
    struct list_head dirty;      // on the dirty list
};
```

This pattern is everywhere in `drivers/gpu/drm/amd/amdgpu/`. Master it.

## Circular linked list

The "tail" `next` points back to the head. Useful for round-robin.

The kernel `list_head` is **already circular**: an empty list has `head.next == head.prev == &head`. This makes many edge cases (empty/non-empty) collapse — no NULL checks needed.

## When linked lists actually beat arrays

Arrays are usually faster (cache!). Linked lists win when:

- **You hold pointers to interior elements** and need O(1) removal.
- **Splicing** entire sublists in/out is needed (O(1) for linked, O(n) for array).
- **Memory fragmentation** prevents one big allocation.

Real GPU example: the DRM scheduler keeps queued jobs on a `list_head`. Jobs may be canceled or reordered — linked list handles that cheaply.

## Common bugs

1. **Forgetting to update both `prev` and `next`** on doubly-linked deletions.
2. **Use-after-free** — freeing a node mid-iteration. Use `list_for_each_entry_safe` in the kernel; in userspace, store `next` before freeing.
3. **Memory leak** — failing to walk and free every node.
4. **Cycle introduction** — splicing wrong creates infinite loops.
5. **Pointer aliasing** — two `list_head`s pointing into same node; almost always a bug.

## Iterate-while-deleting pattern (very common, easy to get wrong)

Wrong:
```c
for (node_t *p = head; p; p = p->next) {
    free(p);    // we just freed p; p->next is reading freed memory
}
```

Right:
```c
node_t *p = head;
while (p) {
    node_t *next = p->next;
    free(p);
    p = next;
}
```

Kernel idiom:
```c
struct task *t, *tmp;
list_for_each_entry_safe(t, tmp, &mylist, node) {
    list_del(&t->node);
    kfree(t);
}
```

## Try it

1. Implement `list_t` (singly linked) with: `push_front`, `push_back` (with O(1) tail), `pop_front`, `find`, `remove(value)`, `print`, `free`.
2. Implement reverse (iterative). Test on lists of size 0, 1, 2, 100.
3. Implement Floyd's cycle detection. Construct a cycle deliberately and detect it.
4. Implement a doubly-linked list with the **embedded link** style (like Linux's `list_head`). Use `container_of` to go from link → containing struct.
5. Read `include/linux/list.h`. Identify these macros: `list_add`, `list_del`, `list_for_each_entry`, `list_for_each_entry_safe`, `list_empty`, `list_first_entry`. Try to read `list_for_each_entry` and decode it.
6. Bonus: implement an LRU cache using a hash map + doubly linked list. (We'll do this fully in DSA-26.)

## Read deeper

- `include/linux/list.h` — read the whole file. It's short and gold.
- `Documentation/core-api/kernel-api.rst` — kernel API list helpers.
- Linus on the kernel list API: search "linus torvalds linked list" — there's a famous lwn.net article.

Next → [Stacks & queues](DSA-05-stacks-queues.md).
