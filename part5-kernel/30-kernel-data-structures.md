# Chapter 30 — Kernel data structures: lists, rbtrees, xarray, idr

The kernel does not use STL. It has its own intrusive data structures, optimized for in-place embedding, no allocation in the hot path, and zero RTTI. These show up everywhere in amdgpu.

## 30.1 The intrusive doubly-linked list

`struct list_head` is the most-used data structure in the kernel.

```c
struct list_head { struct list_head *next, *prev; };
```

It is meant to be **embedded** in your struct:

```c
struct obj {
    int data;
    struct list_head node;
};

LIST_HEAD(my_list);    /* declares + inits an empty list head */

struct obj *o = kmalloc(sizeof *o, GFP_KERNEL);
o->data = 5;
INIT_LIST_HEAD(&o->node);
list_add(&o->node, &my_list);
```

To iterate:

```c
struct obj *o;
list_for_each_entry(o, &my_list, node) {
    pr_info("%d\n", o->data);
}
```

Macros:

- `list_add(new, head)` — front.
- `list_add_tail(new, head)` — back.
- `list_del(node)` — remove.
- `list_empty(head)` — is empty.
- `list_first_entry(head, type, member)` — first.
- `list_entry(ptr, type, member)` — same as `container_of`.
- `list_for_each_entry_safe(pos, n, head, member)` — safe to delete current.
- `list_splice` — merge lists.

The trick is `container_of`: `list_for_each_entry` walks `list_head` pointers but yields `struct obj *` because it computes the offset at compile time.

amdgpu uses lists for: BO lists, fence lists, IRQ source lists, scheduler entities, idle queues.

## 30.2 hlist — hashed linked list

`struct hlist_head` is a single-pointer head for buckets in hash tables. Same idea as `list_head`, smaller memory.

`include/linux/hashtable.h` provides static-size hash tables:

```c
DEFINE_HASHTABLE(table, 8);    /* 256 buckets */
hash_init(table);
hash_add(table, &obj->node, obj->key);
struct obj *o;
hash_for_each_possible(table, o, node, key) {
    if (o->key == key) /* match */;
}
```

## 30.3 Red-black trees (`rb_tree`)

For ordered lookups, the kernel has `struct rb_root` and `struct rb_node`. Self-balancing BST.

```c
struct obj { int key; struct rb_node node; };

struct rb_root tree = RB_ROOT;

static int insert(struct obj *o) {
    struct rb_node **p = &tree.rb_node, *parent = NULL;
    while (*p) {
        struct obj *t = rb_entry(*p, struct obj, node);
        parent = *p;
        if (o->key < t->key) p = &(*p)->rb_left;
        else if (o->key > t->key) p = &(*p)->rb_right;
        else return -EEXIST;
    }
    rb_link_node(&o->node, parent, p);
    rb_insert_color(&o->node, &tree);
    return 0;
}
```

A bit verbose. The kernel doesn't generate templates, so each user repeats the pattern. amdgpu uses rbtrees for VM (GPU virtual memory) range lookups.

## 30.4 Interval trees

Augmented rb-tree for `[start, last]` intervals. `include/linux/interval_tree.h`. Used for VM area overlaps. amdgpu's `amdgpu_vm.c` uses this for buffer-object placements in GPU virtual memory.

## 30.5 IDR / IDA / xarray

Sparse arrays keyed by integers — useful for "give me a small integer ID for this pointer."

- **`struct ida`** — allocates IDs (just bits).
- **`struct idr`** — pointer ↔ id mapping (older API).
- **`struct xarray`** — modern replacement for `idr` and the radix tree.

```c
DEFINE_XARRAY(my_xa);

int id = xa_alloc(&my_xa, &id, ptr, XA_LIMIT(0, 1024), GFP_KERNEL);
void *p = xa_load(&my_xa, id);
xa_erase(&my_xa, id);
```

GPU drivers use IDR/xarray to give userspace handles to kernel objects. DRM core itself uses them for object handles (BOs, syncobjs, contexts).

## 30.6 Lookup-and-insert atomic patterns

```c
xa_lock(&xa);
void *p = xa_load(&xa, id);
if (!p) {
    p = make();
    __xa_store(&xa, id, p, GFP_KERNEL);
}
xa_unlock(&xa);
```

`xa_lock` is internal; use it for compound operations. For simple ops, the atomic variants suffice.

## 30.7 Bitmaps (review)

`DECLARE_BITMAP(map, N)`, `set_bit`, `clear_bit`, `find_first_zero_bit`. From Chapter 6, but worth restating: when a kernel struct needs to track "which N things are in use" cheaply, a bitmap is the right answer.

## 30.8 The `kfifo`

A kernel SPSC ring buffer:

```c
DECLARE_KFIFO(my_fifo, u8, 4096);

INIT_KFIFO(my_fifo);
kfifo_in(&my_fifo, buf, n);
kfifo_out(&my_fifo, buf, n);
```

Useful when one context produces and one consumes — typical of IRQ → workqueue paths.

## 30.9 The `llist` — lock-less list

`struct llist_head`, `llist_add`, `llist_del_all`. A *singly-linked* lock-free list using CAS. Fast for "many writers, single drainer." Used in workqueue internals and elsewhere.

## 30.10 Reference counting

- **`refcount_t`** — saturating refcount with overflow detection.
- **`struct kref`** — `refcount_t` plus a `release` callback pattern.

```c
struct foo {
    struct kref ref;
    /* … */
};

static void foo_release(struct kref *kref) {
    struct foo *f = container_of(kref, struct foo, ref);
    kfree(f);
}

kref_get(&f->ref);
kref_put(&f->ref, foo_release);
```

Used everywhere ownership is shared. `dma_fence` uses kref. `drm_gem_object` uses kref. `pci_dev` uses kref.

## 30.11 Putting it together — driver-internal indices

A typical driver:

```c
struct mydrv {
    struct mutex     lock;
    struct list_head bo_list;          /* all BOs */
    struct xarray    handles;          /* uint32 → BO  */
    struct rb_root   vm_tree;          /* GPU VM */
    DECLARE_BITMAP(free_qids, 64);     /* which queue IDs free */
};
```

This is a *mini-amdgpu* in five lines. Almost every kernel subsystem composes from these primitives.

## 30.12 Exercises

1. Build a small kernel module that creates 10 `struct obj` with `list_head node` and `int data`. Insert into a list, sort by data using `list_sort`, iterate, free.
2. Build a `struct rb_root` of `(key, value)` pairs. Insert, lookup, delete. Print in order.
3. Use an `xarray` to map small integer handles to dynamically-allocated objects. Allocate IDs, look up, release.
4. Use `kfifo` between a kthread (producer) and a workqueue (consumer); pump 1M bytes through.
5. Read `include/linux/list.h`. It is short and beautiful. Memorize the pattern.

---

Next: [Chapter 31 — Kernel memory: kmalloc, vmalloc, slab, page allocator](31-kernel-memory.md).
