# DSA 4 — Linked lists (read-to-learn version)

> A linked list is a chain of "boxes," each pointing to the next. The Linux kernel uses linked lists in **thousands of places**.

## The big idea

In an **array**, all elements live next to each other in memory. To find element 5, you skip ahead 5 slots (constant time).

In a **linked list**, elements live **anywhere** in memory. Each element has a value **plus a pointer** to the next element. To find element 5, you must follow 5 pointers (linear time).

```
Array (contiguous):

  +-----+-----+-----+-----+-----+
  | 10  | 20  | 30  | 40  | 50  |
  +-----+-----+-----+-----+-----+
  ↑
  one block of memory


Linked list (scattered):

  head ──▶ [10|next]──▶ [20|next]──▶ [30|next]──▶ [40|next]──▶ [50|next]──▶ NULL
            (addr 1000)  (addr 2540)  (addr 700)   (addr 9020)  (addr 4400)

  Each node is its own little allocation, anywhere in memory.
  They're connected only via the next pointer.
```

The dashes `──▶` are **pointers** — addresses of the next box.

`head` is a single pointer that holds the address of the first node. To walk the list, you follow `head → next → next → next → ... → NULL`. **NULL** marks the end.

## When linked lists win, when arrays win

| | Array | Linked list |
|---|---|---|
| Random access (`arr[i]`) | **O(1)** | **O(n)** (must walk) |
| Insert at front | **O(n)** (shift all) | **O(1)** (just relink) |
| Delete a node you have a pointer to | **O(n)** (shift) | **O(1)** (relink) |
| Memory layout | contiguous, cache-friendly | scattered, cache-unfriendly |
| Memory overhead per element | none | one pointer per node (8 bytes on 64-bit) |

**Use a linked list** when you do many insertions/deletions in the middle and have direct pointers to nodes.
**Use an array** for random access or tight loops where cache speed matters.

## The struct that defines a node

```c
typedef struct node {
    int val;
    struct node *next;
} node_t;
```

A node holds a value (here, an int) and a pointer to the next node.

In memory each node looks like:

```
            +----+--------+
            | val|  next  |   ← 16 bytes total (4 + 4 padding + 8 ptr on x86_64)
            +----+--------+
              ↑       ↑
              data    pointer to next node (or NULL)
```

## Building a list, step by step

We want to build the list `[10, 20, 30]` by **pushing each at the front**.

### Step 1: empty list

```c
node_t *head = NULL;
```

```
head ──▶ NULL
```

### Step 2: push 30 at the front

```c
node_t *n1 = malloc(sizeof(node_t));
n1->val  = 30;
n1->next = head;     // currently NULL
head     = n1;
```

```
head ──▶ [30|next]──▶ NULL
```

### Step 3: push 20 at the front

```c
node_t *n2 = malloc(sizeof(node_t));
n2->val  = 20;
n2->next = head;     // points to the 30 node
head     = n2;
```

Picture:

```
                 +---------+        +---------+
head ──▶  n2 ──▶ |20 | next|──▶ n1 ─▶|30 | next|──▶ NULL
                 +---------+        +---------+
```

`head` now points to n2, which points to n1, which points to NULL.

### Step 4: push 10 at the front

```c
node_t *n3 = malloc(sizeof(node_t));
n3->val  = 10;
n3->next = head;     // points to the 20 node
head     = n3;
```

Final:

```
head ──▶ [10|next]──▶ [20|next]──▶ [30|next]──▶ NULL
```

That's the list `[10, 20, 30]`. **3 mallocs**, no array resizing, ever.

## list_push_front (in code)

```c
int list_push_front(node_t **head, int v) {
    node_t *n = malloc(sizeof(node_t));   // (1) allocate
    if (!n) return -1;
    n->val  = v;                          // (2) fill in value
    n->next = *head;                      // (3) link to old head
    *head   = n;                          // (4) update head
    return 0;
}
```

Why `node_t **head` (pointer to pointer)? Because we need to **change** what `head` points to. If we wrote `node_t *head` (single pointer), we'd just modify a copy of the pointer — the caller's `head` wouldn't change.

### Trace: push 10, 20, 30 onto an empty list

We track `head` (caller's variable) and `*n` (newly allocated node) at each step.

| Push | Action | Memory after |
|---|---|---|
| start | head = NULL | `head ─▶ NULL` |
| 30 | malloc node A; A.val=30; A.next=NULL; head=A | `head ─▶ [30,NULL]` |
| 20 | malloc node B; B.val=20; B.next=A; head=B | `head ─▶ [20]─▶[30,NULL]` |
| 10 | malloc node C; C.val=10; C.next=B; head=C | `head ─▶ [10]─▶[20]─▶[30,NULL]` |

## Walk the list (find a value)

```c
int list_contains(node_t *head, int v) {
    for (node_t *p = head; p != NULL; p = p->next)
        if (p->val == v)
            return 1;
    return 0;
}
```

`p` walks: starts at `head`, then `p->next`, then `p->next->next`, until `NULL`.

### Trace: search for 20 in `[10, 20, 30]`

| Step | p points to | p->val | match? | next action |
|---|---|---|---|---|
| start | node holding 10 | 10 | no | p = p->next |
| | node holding 20 | 20 | **yes** | return 1 |

Two pointer-follows. **O(n)** time.

### Trace: search for 99

| Step | p points to | p->val | match? | next action |
|---|---|---|---|---|
| start | node holding 10 | 10 | no | p = p->next |
| | node holding 20 | 20 | no | p = p->next |
| | node holding 30 | 30 | no | p = p->next |
| | NULL |   |   | exit loop |

Return 0 (not found). Walked the whole list — **n** pointer-follows.

## Reverse a linked list (the famous interview question)

Given `[10, 20, 30] → NULL`, return `[30, 20, 10] → NULL`.

We use **three pointers**: `prev`, `cur`, `next`. The trick: at each step, **flip cur's next pointer** to point to `prev` instead of forward.

```c
node_t *reverse(node_t *head) {
    node_t *prev = NULL;
    node_t *cur  = head;
    while (cur != NULL) {
        node_t *next = cur->next;   // (1) save the next node before we lose it
        cur->next = prev;            // (2) flip the arrow backward
        prev = cur;                  // (3) advance prev
        cur  = next;                 // (4) advance cur
    }
    return prev;                    // new head
}
```

### Trace step by step on `[10] ─▶ [20] ─▶ [30] ─▶ NULL`

We'll show `prev`, `cur`, `next` at each iteration, plus the **state of links**.

**Initial state:**

```
prev = NULL
cur  ──▶ [10]──▶[20]──▶[30]──▶NULL
```

**Iteration 1:**

```c
next = cur->next;     // next ─▶ [20]
cur->next = prev;     // [10] ─▶ NULL
prev = cur;           // prev ─▶ [10]
cur  = next;          // cur  ─▶ [20]
```

```
After iter 1:
        [10]──▶NULL                   (broken off)
prev ──▶ [10]
cur  ──▶ [20]──▶[30]──▶NULL
```

**Iteration 2:**

```c
next = cur->next;     // next ─▶ [30]
cur->next = prev;     // [20] ─▶ [10]
prev = cur;           // prev ─▶ [20]
cur  = next;          // cur  ─▶ [30]
```

```
After iter 2:
        [20]──▶[10]──▶NULL            (the reversed part so far)
prev ──▶ [20]
cur  ──▶ [30]──▶NULL
```

**Iteration 3:**

```c
next = cur->next;     // next = NULL
cur->next = prev;     // [30] ─▶ [20]
prev = cur;           // prev ─▶ [30]
cur  = next;          // cur  = NULL
```

```
After iter 3:
[30]──▶[20]──▶[10]──▶NULL
prev ──▶ [30]
cur  = NULL              ← loop exits
```

Return `prev` = `[30] ─▶ [20] ─▶ [10] ─▶ NULL`. **Reversed!** ✓

### Why each line of code matters

| Line | Why we need it |
|---|---|
| `next = cur->next` | We're about to overwrite `cur->next`. Save it first or we lose the rest of the list. |
| `cur->next = prev` | The actual reversal — flip this arrow backward. |
| `prev = cur` | Move `prev` forward to the just-reversed node. |
| `cur = next` | Move `cur` forward to keep walking. |

Time: O(n). Space: O(1). **Memorize this code.** It's asked in every interview.

## Detect a cycle (Floyd's tortoise & hare)

If a list has a cycle (some `next` loops back to a previous node), naive walk **never ends**.

The cycle detector uses **two pointers**: slow (steps 1) and fast (steps 2). If there's a cycle, the fast catches up to slow and they meet inside the loop.

```c
int has_cycle(node_t *head) {
    node_t *slow = head, *fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return 1;     // they met → cycle
    }
    return 0;                           // fast hit NULL → no cycle
}
```

### Why fast catches slow

Imagine you're on a circular running track. Two runners start together — one runs at 1 lap/min, one at 2 laps/min. After enough time, the fast runner laps the slow one. They'll be at the same point.

### Trace on `[10] ─▶ [20] ─▶ [30] ─▶ [40] ─▶ [20]` (cycle at 20)

```
[10] ─▶ [20] ─▶ [30] ─▶ [40] ─┐
         ↑                     │
         └─────────────────────┘
```

| Step | slow at | fast at | slow == fast? |
|---|---|---|---|
| start | 10 | 10 | yes (but we haven't started looping; first move) |
| 1 | 20 | 30 | no |
| 2 | 30 | 20 | no |
| 3 | 40 | 40 | **yes — return 1** |

Three steps, cycle found. ✓

### Trace on a list without a cycle

`[10] ─▶ [20] ─▶ [30] ─▶ NULL`

| Step | slow at | fast at |
|---|---|---|
| 1 | 20 | 30 |
| 2 | 30 | NULL |

`fast == NULL` → loop exits, return 0. ✓

## Doubly linked list

Each node has **both** `prev` and `next`:

```c
typedef struct dnode {
    int val;
    struct dnode *prev, *next;
} dnode_t;
```

Picture (5 nodes connected in both directions):

```
     ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐
NULL←┤  10 ├←┤  20 ├←┤  30 ├←┤  40 ├←┤  50 ├ → NULL
     │     │→│     │→│     │→│     │→│     │
     └─────┘ └─────┘ └─────┘ └─────┘ └─────┘
       ↑                              ↑
       head                           tail
```

**Why bother with double the pointers?**

- O(1) deletion of a known node (no need to scan to find its predecessor).
- O(1) walk backward.

**Cost:** one extra pointer per node = 8 more bytes. For million-node lists with tiny payloads, that's 8 MB extra. Trade-off.

## The Linux kernel's `list_head` (the most important DS in the kernel)

In `include/linux/list.h`:

```c
struct list_head {
    struct list_head *next, *prev;
};
```

That's the **whole** struct: just two pointers, no data field! How can a list contain things if the node has no data?

**The trick:** you embed `list_head` inside your own struct.

```c
struct task {
    int pid;
    char name[16];
    struct list_head node;   // embedded link
};
```

Now `struct task` has its own data (`pid`, `name`) AND a `list_head` field for joining lists.

A list of tasks looks like:

```
list_head head ──▶ [task#1.node] ──▶ [task#2.node] ──▶ [task#3.node] ──▶ back to head

Where each task has more fields:

   task#1: { pid=100, name="bash",  node = {prev,next} }
   task#2: { pid=200, name="vim",   node = {prev,next} }
   task#3: { pid=300, name="cat",   node = {prev,next} }
```

The `next`/`prev` pointers chain together the **`node`** field of each task — not the task itself.

So how do we recover the **whole task** from a `list_head` pointer? With a tiny piece of arithmetic magic: `container_of`.

### container_of — the kernel's most-used macro

```c
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
```

It says: "given a pointer to the `member` field, subtract the offset of `member` within `type` to get a pointer to the whole `type`."

Picture:

```
struct task layout in memory (offsets in bytes):

  offset 0   :  pid           (4 bytes)
  offset 4   :  padding        (4 bytes)
  offset 8   :  name[16]      (16 bytes)
  offset 24  :  node           (16 bytes - prev + next pointers)
  total size :  40 bytes

If you have ptr → node (offset 24 of some task instance),
then `ptr - 24` gives you ptr → start of that task instance.
```

So `container_of(node_ptr, struct task, node)` returns the task that owns that `node`.

This lets you embed `list_head` in **any** struct without templates or generics. **One linked-list implementation works for everything.** Brilliant.

### Walking a kernel list

```c
struct task *t;
list_for_each_entry(t, &mylist, node) {
    pr_info("pid=%d\n", t->pid);
}
```

The macro `list_for_each_entry` expands to a `for` loop that:
- starts at `mylist.next`,
- on each iteration, applies `container_of` to recover the `struct task`,
- assigns to `t`,
- stops when we cycle back to `&mylist`.

You write your code as if iterating tasks; the macro handles the link arithmetic.

### Why this matters for you

Once you understand `list_head` + `container_of`, **half the kernel reads like prose**. Drivers, schedulers, the GPU stack — all use this pattern.

## Iterating while deleting (the classic trap)

```c
// WRONG
for (node_t *p = head; p; p = p->next) {
    free(p);              // we just freed p; reading p->next is undefined
}
```

After `free(p)`, the memory at `p` is gone. Reading `p->next` is reading freed memory — could crash, could return garbage.

```c
// RIGHT
node_t *p = head;
while (p) {
    node_t *next = p->next;   // save before freeing
    free(p);
    p = next;
}
```

Save the next pointer **before** freeing the current node.

The kernel's `list_for_each_entry_safe` does this for you. Always use the `_safe` variant when deleting during iteration.

## Common bugs (with pictures)

### Bug 1: lost head

```c
node_t *head = ...;
head = head->next;       // we just lost the original head — leak!
```

```
Before:  head ──▶ [10]──▶[20]──▶[30]
After:   head ──▶ [20]──▶[30]
                  
                  [10]   ← still in memory, but unreachable! LEAK
```

### Bug 2: traversal off the end

```c
for (node_t *p = head; p->next != NULL; p = p->next) ...   // wrong condition
```

If `head` is NULL, you dereference NULL on iteration zero. **Crash.**

Right condition: `p != NULL`.

### Bug 3: cycle introduction

```c
node_t *a = ..., *b = ...;
a->next = b;
b->next = a;             // cycle! list_for_each_* will loop forever
```

Always reason about whether your `next` writes break or create cycles.

## Recap

1. A linked list is a chain of nodes, each with value + next pointer.
2. **Push at front: O(1).** Search/index/insert-in-middle (without a pointer to the spot): O(n).
3. **Three operations to memorize**: walk the list, reverse the list, detect a cycle (Floyd's).
4. The Linux kernel uses **embedded `list_head`** + **`container_of`** — read `include/linux/list.h` once you understand chapter 4.
5. When deleting during iteration, **save next first** (or use `_safe` variant).

## Self-check (in your head)

1. You have a 100-node linked list. To access the 50th node, how many pointer follows?

2. Trace `reverse([1, 2])` step by step. What's the final list?

3. In a singly linked list, you have a pointer to the **second-to-last** node. How fast can you delete the **last** node? (O(1) or O(n)?)

4. In the same setup but **doubly** linked, how fast can you delete the **last** node?

5. Why does `container_of` not need to know the type of the field — only the type of the containing struct and the field name?

---

**Answers:**

1. **49 pointer follows** (start at head; that's node 1; follow 49 times to reach node 50). Linear in position.

2. After iter 1: prev → [1] → NULL, cur → [2]. After iter 2: prev → [2] → [1] → NULL, cur = NULL. **Final: [2, 1]** ✓

3. **O(1).** You have the prev; just set `prev->next = NULL`. (Don't forget to free the last node.)

4. Also **O(1).** And easier — the last node points back via `prev`, so given a pointer to the last node, you fix `last->prev->next = NULL` and free.

5. Because `offsetof(type, member)` computes the byte offset at **compile time** — that's all that's needed. The pointer math doesn't care about the field's type, only its offset.

If you got 4/5 you understand linked lists. If 5/5, move on.

Next → [DSA-05 — Stacks and queues](DSA-05-stacks-queues.md).
