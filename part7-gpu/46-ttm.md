# Chapter 46 — TTM — the memory manager amdgpu uses

TTM (Translation Table Manager) is DRM's bigger-brother memory manager, designed for GPUs with VRAM, paging between system memory and VRAM, and eviction under pressure. amdgpu, radeon, nouveau (recent), vmwgfx, qxl all use TTM.

## 46.1 The problem TTM solves

A GPU has finite VRAM. Workloads allocate more BO memory than VRAM. We must:

- Place BOs where they're cheapest to access ("preferred placement").
- **Evict** lower-priority BOs from VRAM to GTT when needed.
- Migrate transparently with respect to in-flight GPU work.
- Track per-BO **fences** so we don't evict something currently being used.

TTM does all of this generically.

## 46.2 The pieces

```
   ttm_resource_manager  per-pool placement (VRAM, GTT, …)
        │
   ttm_buffer_object     one BO; embeds drm_gem_object
        │
   ttm_resource          where this BO currently lives
        │
   ttm_tt                CPU-side page array (system RAM)
```

- **`ttm_buffer_object` (TBO)**: the BO type. Driver-specific BO embeds it.
- **`ttm_resource_manager`**: per-pool allocator (one for VRAM, one for GTT, etc.).
- **`ttm_resource`**: the *current* placement; pointer to the manager and within-pool offset.
- **`ttm_tt`**: backing pages when in system memory.

amdgpu has:

- `mgr_vram` for VRAM (a buddy allocator).
- `mgr_gtt` for GTT (linear allocator with eviction).
- `mgr_doorbell`, `mgr_oa`, etc. for special pools.

## 46.3 Placement

A `ttm_placement` struct says "this BO is allowed in these pools, prefer that one":

```c
struct ttm_place {
    uint32_t  fpfn, lpfn;   /* page-frame range */
    uint32_t  mem_type;     /* TTM_PL_VRAM | TTM_PL_TT | ... */
    uint32_t  flags;
};

struct ttm_placement {
    unsigned          num_placement;
    const struct ttm_place *placement;
    unsigned          num_busy_placement;
    const struct ttm_place *busy_placement;
};
```

`busy_placement` is fallback when the primary is full and can't evict.

## 46.4 Migration and fences

When TTM moves a BO from VRAM → GTT, it issues a GPU copy job (using SDMA — fast, asynchronous). The copy returns a fence. TTM hangs the fence on the BO. Subsequent GPU work that touches the BO will wait on the fence.

If the BO is being read while moving: TTM serializes via `dma_resv` (Chapter 45).

Driver supplies a `move()` callback that knows how to copy. `amdgpu_bo_move()` issues an SDMA copy and returns a fence.

## 46.5 Eviction

When `mgr_vram` runs out:

- It picks a victim (LRU, lowest priority, not pinned).
- Calls `ttm_bo_validate(victim, &placement_to_GTT, ctx)` — moves it.
- Now there's space; the requested allocation succeeds.

Eviction is the single subtlest path in any GPU driver. Bugs here look like graphical glitches, hangs under memory pressure, or "the GPU reset itself."

## 46.6 The "reservation" lock

Every BO has a `dma_resv` whose lock is a `ww_mutex` — a "wait-wound" mutex that supports deadlock-free multi-lock acquisition (you can lock 100 BOs at once and TTM ensures progress).

When you bind multiple BOs for a CS, you take the wound-mutex on all of them via `ttm_eu_reserve_buffers`. If a deadlock would occur, you "wound" yourself, drop and retry — guaranteed to eventually succeed.

This is one of the cleverest pieces of synchronization in the kernel.

## 46.7 The amdgpu_bo struct (real)

```c
struct amdgpu_bo {
    struct ttm_buffer_object        tbo;
    struct ttm_bo_kmap_obj          kmap;
    u64                             flags;
    unsigned                        prime_shared_count;
    struct list_head                shadow_list;
    struct amdgpu_bo               *parent;
    struct amdgpu_bo               *shadow;
    struct amdgpu_mn               *mn;
    union {
        struct dma_buf_attachment  *attach;
        struct {
            struct drm_mm_node     *bin;       /* binning */
            uint64_t                preferred_domains;
            uint64_t                allowed_domains;
        };
    };
    /* … */
};
```

Many fields. The key ones:

- `tbo`: TTM half.
- `flags`: BO flags (no-eviction, contiguous, persistent).
- `parent`: for sub-allocated regions (sub-allocator atop a big BO).
- `mn`: MMU notifier for userptr BOs.

## 46.8 The buddy VRAM allocator

amdgpu's VRAM manager is a buddy. Same structure as the kernel's page allocator. Free lists by power-of-two block sizes; split on alloc, merge on free.

Live in `drivers/gpu/drm/amd/amdgpu/amdgpu_vram_mgr.c`. Worth reading after this textbook.

## 46.9 Sub-allocator

Many small descriptor-sized allocations would fragment VRAM. amdgpu's `amdgpu_sa_bo` carves a single big BO into smaller chunks.

```c
struct amdgpu_sa_manager {
    struct amdgpu_bo  *bo;
    struct list_head   olist;     /* in-use chunks */
    struct list_head   flist[…];  /* free chunks per ring */
    /* … */
};
```

Used for IBs and small kernel-internal structures.

## 46.10 Performance pitfalls

- **Migration thrash**: a BO bouncing between VRAM and GTT under pressure. Symptom: low FPS that improves when VRAM has slack. Triage: instrument migrations with tracepoints.
- **Eviction deadlock**: two CSes each need each other's BO evicted. ww_mutex prevents true deadlock but you may stall. Watch fence wait latencies.
- **Pinned-too-long**: scanout BOs pinned, fragmentation grows. Mitigate with DC's "underflow recovery" path.

## 46.11 The kernel call you'll see most

```c
amdgpu_bo_create(adev, &bp, &bo);
amdgpu_bo_reserve(bo, false);
amdgpu_bo_pin(bo, AMDGPU_GEM_DOMAIN_VRAM);
amdgpu_bo_kmap(bo, &cpu);
/* … use … */
amdgpu_bo_kunmap(bo);
amdgpu_bo_unpin(bo);
amdgpu_bo_unreserve(bo);
amdgpu_bo_unref(&bo);
```

That's the rhythm of every kernel-internal BO use.

## 46.12 Exercises

1. Read `drivers/gpu/drm/ttm/ttm_bo.c` — focus on `ttm_bo_validate` and `ttm_bo_move`.
2. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_vram_mgr.c`. Identify the buddy structure.
3. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_sa.c`. Identify the suballocator.
4. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_object.c::amdgpu_bo_create`. Trace through what flags drive what placements.
5. Run a workload that exhausts VRAM (`vkmark` heavy textures, or `dmesg`-watch under games). Observe migration and eviction in `/sys/kernel/debug/dri/0/`.

---

Next: [Chapter 47 — KMS — display, planes, CRTCs, atomic modesetting](47-kms.md).
