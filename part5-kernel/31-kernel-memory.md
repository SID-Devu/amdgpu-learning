# Chapter 31 — Kernel memory: kmalloc, vmalloc, slab, page allocator

The kernel has its own allocators with their own rules. Picking the wrong one leaks memory, crashes the system, or DMAs garbage. This chapter is short, concrete, and one you'll re-read often.

## 31.1 The hierarchy

```
                     ┌───────────────────────────┐
                     │      Buddy allocator      │   physical pages
                     │  alloc_pages, free_pages  │   1, 2, 4, …, 1024 pgs
                     └────────────┬──────────────┘
                                  │
              ┌───────────────────┼─────────────────────┐
              ▼                                         ▼
   ┌────────────────────┐                      ┌──────────────────┐
   │ Slab / SLUB / SLOB │                      │     vmalloc      │
   │  caches of fixed   │                      │ contiguous virt, │
   │  small objects     │                      │ scattered phys   │
   └─────────┬──────────┘                      └─────────┬────────┘
             │                                           │
             ▼                                           ▼
        kmalloc(size, flags)                      vmalloc(size)
        kmem_cache_alloc(c, flags)
```

Above kmalloc lives `dma_alloc_coherent`, `dma_pool`, special arenas — used in driver paths. We get there in Part VI.

## 31.2 GFP flags

Every allocation specifies what it can do:

| Flag | Meaning |
|---|---|
| `GFP_KERNEL` | normal, may sleep, may swap |
| `GFP_ATOMIC` | no sleep — for IRQ/atomic context |
| `GFP_NOWAIT` | no sleep, no oom-kill |
| `GFP_DMA` | low memory zone (legacy 16MB) |
| `GFP_DMA32` | < 4 GiB physical for 32-bit DMA |
| `__GFP_ZERO` | zero memory |
| `__GFP_NOWARN` | suppress oom warnings |
| `__GFP_HIGH` | use emergency reserves |
| `GFP_HIGHUSER_MOVABLE` | userspace pages, movable |

You can combine: `GFP_KERNEL | __GFP_ZERO`. `kzalloc` is shorthand.

In atomic context (IRQ, spinlock-held) you **must** use `GFP_ATOMIC`. Using `GFP_KERNEL` will warn and may deadlock.

## 31.3 `kmalloc` and `kfree`

```c
void *p = kmalloc(size, GFP_KERNEL);
if (!p) return -ENOMEM;
/* … */
kfree(p);
```

`kmalloc` returns physically contiguous memory. Aligned to the *natural alignment* of `size` (or page if size ≥ page). Backed by SLUB caches for small sizes; falls back to `alloc_pages` for big ones. Max size by default ~4 MB; beyond, you want `vmalloc` or `kvmalloc`.

`kzalloc(size, GFP_KERNEL)` = zeroed.

`krealloc(p, new_size, flags)` like userland realloc.

## 31.4 `kvmalloc`

Tries `kmalloc` first (contiguous), falls back to `vmalloc` (non-contiguous). Use for buffers that may be > a few pages and whose physical contiguity doesn't matter.

```c
void *p = kvmalloc(size, GFP_KERNEL);
kvfree(p);
```

## 31.5 `vmalloc`

```c
void *p = vmalloc(size);
vfree(p);
```

Returns memory whose **virtual** addresses are contiguous, but **physical** pages may be scattered. Use when:

- size is very large (multi-MB).
- physical contiguity isn't needed.
- you're willing to pay TLB cost (no huge pages).

Cannot be DMA'd from a typical PCI device (which expects contiguous physical or scatter-gather).

## 31.6 The slab allocator

`kmalloc` is built on slab caches. For your own fixed-size objects, create a cache:

```c
struct kmem_cache *c = kmem_cache_create("my_obj",
    sizeof(struct my_obj), 0, SLAB_HWCACHE_ALIGN, NULL);

struct my_obj *o = kmem_cache_alloc(c, GFP_KERNEL);
kmem_cache_free(c, o);

kmem_cache_destroy(c);
```

Benefits over `kmalloc`: per-CPU magazines, predictable layout, optional constructor, sysfs stats (`/sys/kernel/slab/my_obj`).

amdgpu uses slab caches for `dma_fence`, `amdgpu_bo`, `drm_sched_job` etc.

## 31.7 The page allocator

Lower level than `kmalloc`. Returns whole pages.

```c
struct page *p = alloc_pages(GFP_KERNEL, order);
void *addr = page_address(p);
__free_pages(p, order);
```

Order = log₂ of page count. `alloc_pages(GFP_KERNEL, 0)` gives 1 page, `…, 4` gives 16 pages. Internally a buddy allocator.

For a `void *` interface: `__get_free_pages` / `free_pages`.

DMA paths often want pages: `dma_map_page` takes a `struct page *`.

## 31.8 `devm_*` — auto-free on detach

For driver probe-time allocations that must be freed when the device unbinds, the kernel offers `devm_kzalloc`, `devm_kmalloc`, `devm_request_irq`, `devm_kvasprintf`, etc. They auto-clean on driver detach.

```c
struct mydrv *m = devm_kzalloc(dev, sizeof *m, GFP_KERNEL);
m->mmio = devm_ioremap_resource(dev, res);
```

Greatly reduces `goto out_*` chains.

## 31.9 Reading memory pressure

- `cat /proc/meminfo` — overall memory state.
- `cat /proc/slabinfo` — per-cache stats (root only).
- `slabtop` — interactive view.
- `/sys/kernel/slab/<cache>/...` — per-cache stats.
- `vmstat 1` — page activity.

Driver leaks usually show up as "this slab cache keeps growing during long runs."

## 31.10 KASAN, KFENCE, and DEBUG_VM

Build with these in dev:

- **KASAN** — like ASan; detects use-after-free and out-of-bounds in kernel allocations.
- **KFENCE** — sampled, low-overhead use-after-free / OOB; fine for production debug.
- **DEBUG_VM** — many extra invariant checks in MM.
- **DEBUG_PAGEALLOC** — unmaps freed pages so use-after-free oopses immediately.

For driver work: KASAN. Kernel will boot ~30% slower but find your bugs.

## 31.11 The `dma-buf` allocator (preview)

GPU drivers also use DMA-related allocators. We discuss in Chapter 38 and 45:

- `dma_alloc_coherent(dev, size, &dma_handle, gfp)` — coherent DMA buffer (the device sees it consistent without flushing).
- `dma_pool_create()` for many small DMA allocations.
- `dma_map_single` / `dma_map_sg` for streaming DMA on existing memory.

## 31.12 Practical guidance

- Small struct: `kzalloc(sizeof *p, GFP_KERNEL)`.
- Many of one struct: `kmem_cache_create` + `kmem_cache_alloc`.
- 1+ MB buffer not for DMA: `kvmalloc`.
- Multi-MB DMA buffer: `dma_alloc_coherent`.
- Any allocation in IRQ/atomic: `GFP_ATOMIC`, but consider preallocating in process context first.

## 31.13 Exercises

1. In a kernel module, allocate `kmalloc(1024, GFP_KERNEL)`; print the address; free. Try with `GFP_ATOMIC` from a tasklet.
2. Create a slab cache for a small struct; allocate 1000 objects; check `/sys/kernel/slab/<cache>/`; free; check again.
3. Compare physical contiguity: `kmalloc(1<<22)` (4 MB) vs `vmalloc(1<<22)`. Translate to physical via `virt_to_phys` / `vmalloc_to_page` for each.
4. Build a kernel with KASAN; insmod a module that does `int *p = kmalloc(4); kfree(p); *p = 0;`. Read the dmesg splat.
5. Read `mm/slub.c::kmem_cache_alloc` — see the per-CPU magazine fast path.

---

Next: [Chapter 32 — Time, timers, workqueues, kthreads](32-time-and-deferred-work.md).
