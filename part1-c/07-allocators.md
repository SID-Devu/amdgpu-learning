# Chapter 7 — Dynamic memory and writing your own allocator

If you understand allocators, you understand most of operating systems. The kernel itself contains at least four allocators (`buddy`, `slab/SLUB`, `kmalloc`, `vmalloc`) and the GPU stack adds more (`TTM`, `DRM-GEM`, `amdgpu_bo`). This chapter takes you from `malloc` to writing a real free-list allocator.

## 7.1 The libc heap

`malloc(3)`, `calloc(3)`, `realloc(3)`, `free(3)` come from libc. They manage a *heap* — a region of the process's address space that grows on demand:

```c
int *p = malloc(sizeof(int) * 100);
if (!p) abort();
memset(p, 0, sizeof(int) * 100);
free(p);
```

Things you must remember:

- `malloc` may return `NULL`. Always check.
- `malloc` does **not** zero memory. Use `calloc` (which does) or `memset`.
- `realloc(p, n)` can move the buffer — old `p` may become invalid. Always assign the result.
- `free(NULL)` is defined and is a no-op. `free(p)` followed by `free(p)` is UB.

```c
char *grow(char *buf, size_t *cap)
{
    size_t new_cap = *cap ? *cap * 2 : 16;
    char *nb = realloc(buf, new_cap);
    if (!nb) return NULL;        /* keep old buf valid */
    *cap = new_cap;
    return nb;
}
```

## 7.2 Where does memory come from?

`malloc` doesn't talk to hardware. Internally it calls `brk(2)` or `mmap(2)` to get pages from the kernel, then carves them up. The standard glibc allocator (`ptmalloc2`) maintains:

- **Arenas**: per-thread heaps to reduce contention.
- **Bins**: free lists for small/medium/large/huge sizes.
- **Top chunk**: a big slab grown via `sbrk`.

Modern alternatives: `tcmalloc` (Google), `jemalloc` (Facebook/FreeBSD), `mimalloc` (Microsoft). They differ in fragmentation, locality, and concurrency strategy. ROCm runtimes use jemalloc or mimalloc in places.

If you `strace` a libc `malloc(8)` it makes 0 syscalls — the heap is already mapped. `malloc(8 * 1024 * 1024)` may issue an `mmap()`.

## 7.3 Memory layout of a small allocation

A glibc small allocation looks like:

```
                       ┌──────────────────┐
ptr returned to user → │   payload (n)    │
                       └──────────────────┘
                       ┌──────────────────┐
ptr - sizeof(size_t) → │  size + flags    │   "chunk header"
                       └──────────────────┘
```

`free(p)` reads the size from the header *before* `p` to know how much to release. This is why writing past the start (off-by-one underflow) corrupts the heap silently.

This is also why `free` doesn't take a size argument while `kfree` in the kernel doesn't either. `kmalloc` stashes metadata too.

## 7.4 Patterns and anti-patterns

```c
/* Pattern: allocate-or-fail */
struct foo *f = malloc(sizeof *f);
if (!f) return -ENOMEM;

/* Pattern: caller-allocates */
void compute(struct result *out);  /* caller allocates, callee fills */

/* Pattern: borrow */
void process(const struct data *d);  /* callee does not own */

/* Anti-pattern: returning an internal buffer */
const char *get_name(void) {
    static char buf[64];   /* not thread-safe */
    /* … */
    return buf;
}
```

Driver code idiomatic patterns:

- `kzalloc(sizeof *p, GFP_KERNEL)` for "give me a zeroed struct."
- `kmem_cache_alloc()` when you allocate the same size repeatedly.
- `devm_kzalloc(dev, size, GFP_KERNEL)` for "free me when device unbinds."
- `dma_alloc_coherent()` for memory the device will DMA from/to.

We will visit each in Part V.

## 7.5 The arena allocator

When a phase of work has many short-lived allocations and a clear "end of phase," an **arena** is dramatically simpler and faster than malloc. You bump a pointer; you free the whole arena at once.

```c
/* arena.h */
#include <stddef.h>
typedef struct arena {
    char  *base;
    size_t cap;
    size_t off;
} arena;

void  arena_init(arena *a, void *backing, size_t cap);
void *arena_alloc(arena *a, size_t size, size_t align);
void  arena_reset(arena *a);
```

```c
/* arena.c */
#include <assert.h>
#include <stdint.h>
#include "arena.h"

static size_t align_up(size_t x, size_t a) {
    return (x + a - 1) & ~(a - 1);
}

void arena_init(arena *a, void *backing, size_t cap) {
    a->base = backing;
    a->cap  = cap;
    a->off  = 0;
}

void *arena_alloc(arena *a, size_t size, size_t align) {
    assert((align & (align - 1)) == 0);
    size_t off = align_up(a->off, align);
    if (off + size > a->cap) return NULL;
    void *p = a->base + off;
    a->off  = off + size;
    return p;
}

void arena_reset(arena *a) { a->off = 0; }
```

Use:

```c
char backing[1 << 16];
arena a;
arena_init(&a, backing, sizeof backing);
int *xs = arena_alloc(&a, sizeof(int)*100, _Alignof(int));
arena_reset(&a);   /* all 100 ints "freed" in O(1) */
```

This is the same pattern as the kernel's `mempool`, `dma_pool`, and (loosely) the GPU command buffer allocator. Compilers, IR optimizers, JITs all use arenas.

## 7.6 A free-list allocator

A real general-purpose allocator must support free of individual blocks. The simplest model that's worth writing yourself:

- Each block has a header: `{size, free_flag, prev_block_size}`.
- A *free list* threads through free blocks by their headers.
- `alloc(n)` walks the free list, finds a fit, splits if too large.
- `free(p)` flips the flag and *coalesces* with adjacent free blocks.

```c
/* freelist_alloc.c — illustrative; not multi-thread safe. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define ALIGN 16

typedef struct hdr {
    size_t size;     /* including header */
    bool   free;
    struct hdr *prev;
    struct hdr *next;   /* in free list */
} hdr;

static char   pool[1 << 20];
static hdr   *free_head;

static size_t align_up(size_t x) { return (x + ALIGN - 1) & ~(ALIGN - 1); }

void fl_init(void) {
    free_head = (hdr *)pool;
    free_head->size = sizeof pool;
    free_head->free = true;
    free_head->prev = NULL;
    free_head->next = NULL;
}

void *fl_alloc(size_t n) {
    size_t need = align_up(n + sizeof(hdr));
    for (hdr *h = free_head; h; h = h->next) {
        if (!h->free || h->size < need) continue;
        if (h->size >= need + sizeof(hdr) + ALIGN) {
            hdr *split = (hdr *)((char *)h + need);
            split->size = h->size - need;
            split->free = true;
            split->prev = h;
            split->next = h->next;
            if (h->next) h->next->prev = split;
            h->size = need;
            h->next = split;
        }
        h->free = false;
        return (char *)h + sizeof(hdr);
    }
    return NULL;
}

void fl_free(void *p) {
    if (!p) return;
    hdr *h = (hdr *)((char *)p - sizeof(hdr));
    h->free = true;
    /* coalesce forward */
    if (h->next && h->next->free) {
        h->size += h->next->size;
        h->next = h->next->next;
        if (h->next) h->next->prev = h;
    }
    /* coalesce backward */
    if (h->prev && h->prev->free) {
        h->prev->size += h->size;
        h->prev->next = h->next;
        if (h->next) h->next->prev = h->prev;
    }
}
```

This is missing the threading the free list separately from the header chain (real allocators use a separate free list to walk only free blocks), and it's not thread-safe, but it captures the *shape*. A weekend of polishing turns this into a teaching allocator. Implementing one is the single best way to understand `kmalloc`/`SLUB`.

## 7.7 The SLUB / slab idea

For fixed-size, frequently-allocated objects, scanning a free list is wasteful. The slab allocator pre-allocates a *page* of identical objects and hands them out with a single bump or LIFO pop. Free goes back to the slab.

The kernel's `kmem_cache_create()` builds one. You allocate objects from that cache with `kmem_cache_alloc()` and free with `kmem_cache_free()`. This is how `task_struct`, `inode`, `skbuff`, `dma_fence` etc. are managed. We'll use it in Part V.

## 7.8 Lifetimes: who owns the memory?

Every allocation has an *owner* — the code path responsible for `free`. Encode ownership in your API:

- "out parameter, caller frees": `int parse(const char *src, char **out)` — caller frees `*out`.
- "caller-allocated": `int parse_into(const char *src, char *out, size_t out_cap)`.
- "borrowed": `void log_msg(const char *msg)` — log doesn't free; caller still owns.

Kernel uses *reference counting* (`kref`, `refcount_t`) for objects with multiple owners:

```c
struct file {
    refcount_t   ref;
    /* … */
};
```

Each user takes a reference; when the last goes away, the destructor runs. We'll use refcounts heavily for DMA buffers and fences.

## 7.9 Defensive habits

1. After `free(p)`, set `p = NULL`. Then a use-after-free will crash on the next deref instead of corrupting silently.
2. Allocate one struct at a time; don't reuse buffers for unrelated purposes.
3. Use ASan/UBSan/Valgrind in dev. Use `kasan` and `kfence` in your kernel builds.
4. Never trust a length from userspace until you've verified it.

## 7.10 Exercises

1. Implement `arena.h/c` exactly as above. Write a test that allocates 1000 random-sized blocks, then resets the arena, and verifies pointers no longer alias the allocator state.
2. Implement the free-list allocator. Test it: allocate 10000 blocks of random sizes, free half at random, allocate 5000 more, verify no overlap.
3. Add coalescing tests: allocate three adjacent blocks A, B, C. Free A, then C, then B. Verify the free list collapses to a single block.
4. Add a `slab` allocator on top of your arena. `slab_create(size_t obj_size)` returns a slab; `slab_alloc()` returns a fixed-size object; `slab_free()` returns it.
5. Run all of the above under ASan to confirm no overruns.

---

Next: [Chapter 8 — Preprocessor, headers, linkage, build systems](08-preprocessor-and-build.md).
