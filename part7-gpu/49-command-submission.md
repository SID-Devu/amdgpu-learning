# Chapter 49 — Command submission, IBs, rings, doorbells

This is the path on which every game, every CUDA-equivalent compute kernel, every Vulkan submit travels. Master it and you understand how AMD GPUs are programmed from Linux.

## 49.1 The big picture

```
userspace builds an IB (PM4/SDMA packets) in a BO
   │
   │ DRM_IOCTL_AMDGPU_CS (chunks: IBs, deps, syncobjs, BO list)
   ▼
amdgpu_cs_ioctl
   ├── parse chunks
   ├── reserve BOs (ww_mutex via ttm_eu)
   ├── validate IBs (whitelist)
   ├── build drm_sched_job
   ├── hand to drm-sched (Chapter 50)
   │
   ▼
drm_sched picks job ready
   ├── waits on input fences
   ├── runs `run_job` → amdgpu_job_run
   ▼
amdgpu_job_run
   ├── emits prologue (cache flushes, VM switch, etc.)
   ├── pushes IB packet onto HW ring
   ├── emits fence packet
   ├── kicks doorbell
   │
   ▼
GPU CP fetches packets, executes IBs
   ├── eventually reaches fence packet
   ├── writes seqno to memory + raises IRQ
   ▼
amdgpu_fence_process → signals dma_fence → wakes waiters
```

We go through each piece.

## 49.2 An IB (Indirect Buffer)

An IB is a buffer of GPU packets the userspace driver builds. The kernel-side `amdgpu_ib`:

```c
struct amdgpu_ib {
    struct amdgpu_sa_bo  *sa_bo;
    uint32_t              length_dw;
    uint64_t              gpu_addr;
    uint32_t             *ptr;
    uint32_t              flags;
};
```

The IB lives in CPU-mapped memory (sub-allocated). The CPU writes `ptr[i] = packet`; on submit, the driver tells the ring "go execute the IB at `gpu_addr` for `length_dw` dwords."

PM4 example: write 0xCAFEBABE to memory at GPU VA 0x1000:

```c
ptr[0] = PACKET3(PACKET3_WRITE_DATA, 3);
ptr[1] = WRITE_DATA_DST_SEL(WRITE_DATA_DST_SEL_MEM)|WRITE_DATA_ENGINE_SEL(WRITE_DATA_ENGINE_SEL_ME);
ptr[2] = lower_32_bits(0x1000);
ptr[3] = upper_32_bits(0x1000);
ptr[4] = 0xCAFEBABE;
ib->length_dw = 5;
```

In practice userspace builds these. Mesa has `radeonsi_emit_*` and ACO compiles shaders into IB-embedded packets.

## 49.3 The ring

A ring is a circular buffer in memory (CPU- and GPU-readable). The CPU writes packets at `wptr`; the GPU's CP reads at `rptr`. Hardware registers hold the pointers.

```c
struct amdgpu_ring {
    volatile uint32_t  *ring;     /* CPU map */
    uint64_t            gpu_addr; /* GPU VA */
    unsigned            ring_size;
    unsigned            wptr, rptr;
    /* … */
};
```

Pushing a packet:

```c
amdgpu_ring_write(ring, packet);
/* increments wptr */
```

After all packets:

```c
amdgpu_ring_commit(ring);   /* writes wptr to hardware MMIO */
```

The kernel uses `amdgpu_ring_emit_*` helpers per ring type. PM4 packets go to GFX/compute rings; SDMA packets to SDMA rings; VCN packets to VCN rings. Each ring's `funcs` knows how.

## 49.4 The CS ioctl in detail

User struct (simplified):

```c
struct drm_amdgpu_cs_in {
    uint32_t  ctx_id;
    uint32_t  bo_list_handle;
    uint32_t  num_chunks;
    uint32_t  flags;
    uint64_t  chunks;          /* user array of chunk pointers */
};

struct drm_amdgpu_cs_chunk {
    uint32_t  chunk_id;
    uint32_t  length_dw;
    uint64_t  chunk_data;       /* user pointer */
};
```

Chunks include:

- `IB`: IB descriptor (which engine, which BO/offset, length).
- `DEPENDENCIES`: input fences from same context.
- `SYNCOBJ_IN`/`SYNCOBJ_OUT`: external syncobj deps and outputs.
- `FENCE`: where to put the user-fence value on completion.
- `BO_HANDLES`: inline BO list.

Kernel `amdgpu_cs_ioctl`:

```c
int amdgpu_cs_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
    struct amdgpu_cs_parser parser = { 0 };
    int r;

    r = amdgpu_cs_parser_init(&parser, ...);     /* alloc context */
    r = amdgpu_cs_parser_bos(&parser);           /* validate + reserve BOs */
    r = amdgpu_cs_parser_pass1(&parser);         /* parse IB chunks */
    r = amdgpu_cs_parser_pass2(&parser);         /* parse dep/syncobj chunks */
    r = amdgpu_cs_dependencies(&parser);         /* attach input fences */
    r = amdgpu_cs_submit(&parser, ...);          /* push to scheduler */
    amdgpu_cs_parser_fini(&parser);
    return r;
}
```

(This is conceptual; modern code is split across `amdgpu_cs.c` and tweakable.)

## 49.5 BO reservation with ww_mutex

```c
ttm_eu_reserve_buffers(&parser->ticket, &parser->validated, true, &parser->fences_to_wait);
```

This locks every BO in the parser's list using a "wait-wound" protocol. It guarantees forward progress even when many CSes contend.

After reservation, the driver:

- Validates each BO (moves it to a domain the engine can access; e.g., GTT for SDMA).
- Reads in-flight fences.
- Adds the new submission's fence to each BO via `dma_resv_add_fence`.

Upon submission completion, the BOs' resv lists are updated and other CSes that were waiting can proceed.

## 49.6 Validation

The kernel must ensure userspace cannot:

- Read/write privileged registers.
- DMA outside its own BOs.
- Access another process's memory.

amdgpu validates IBs by:

- A **whitelist** for register addresses in `WRITE_DATA`/`SET_*` packets.
- BO-list checks: every memory address used must be backed by a BO the userspace owns.
- VMID isolation: each process has its own VMID; the GPU's MMU enforces.

This validation is the most security-critical code in amdgpu. CVEs here are kernel-level.

## 49.7 Building the drm_sched_job

```c
struct drm_sched_job {
    struct list_head        list;
    struct dma_fence        s_fence;
    struct dma_fence_cb     finish_cb;
    /* … */
};
```

The driver wraps with `struct amdgpu_job` (extends `drm_sched_job`).

```c
amdgpu_job_alloc(adev, num_ibs, &job, ...);
job->ibs[0] = ib;
amdgpu_job_submit(job, entity, ...);
```

`drm_sched_job_init`, `drm_sched_job_arm`, `drm_sched_entity_push_job`. We treat in Chapter 50.

## 49.8 Doorbells

Once the scheduler decides "this job is next," it calls the driver's `run_job(struct drm_sched_job *)` (= `amdgpu_job_run`). The driver:

1. Emits prologue PM4 (VM switch, cache flushes).
2. Pushes the IB packet.
3. Emits a fence packet.
4. Commits the ring (updates `wptr` register or doorbell).

The doorbell write is the magic moment the GPU is told "look, more work." On modern GPUs, doorbells live in MMIO pages mapped per-process; userspace can write directly when using KFD-mode queues.

## 49.9 Multiple engines

amdgpu has multiple rings per engine:

- 1 GFX ring (graphics + compute).
- ~8 compute rings (older parts).
- ~2-4 SDMA rings (system DMA).
- ~2-4 VCN rings (video decode).

The DRM scheduler runs one job per ring at a time. Multiple engines run in parallel.

User can pick which engine via `chunk_ib.ip_type`:

```c
AMDGPU_HW_IP_GFX, AMDGPU_HW_IP_COMPUTE, AMDGPU_HW_IP_DMA,
AMDGPU_HW_IP_UVD, AMDGPU_HW_IP_VCE, AMDGPU_HW_IP_VCN_DEC,
AMDGPU_HW_IP_VCN_ENC, AMDGPU_HW_IP_VCN_JPEG,
```

A modern Vulkan game uses GFX + COMPUTE + DMA + VCN simultaneously.

## 49.10 MES — the firmware scheduler

Newer chips have a MES microcontroller running firmware that does scheduling on-chip. The kernel sends "submit queue X" commands to MES via a mailbox; MES handles preemption, priority, and queue management. KFD's user-mode queues route through MES.

For graphics, MES is being introduced gradually; many chips still use the kernel-side ring driving model.

## 49.11 Performance notes

- Submission latency: 5-50 µs per `amdgpu_cs_ioctl` typical. Userspace queues (KFD) get this down to <1 µs.
- BO list size: large lists cost validation time; userspace can pre-reserve "BO-list handles" and reuse.
- Kernel-side compilation of IBs is zero — they're pre-built in userspace.

## 49.12 Exercises

1. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_cs.c::amdgpu_cs_ioctl`. Walk through all phases.
2. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_ring.c::amdgpu_ring_alloc` and `_commit`.
3. Read `gfx_v10_0.c::gfx_v10_0_ring_emit_ib` to see how an IB packet is composed for GFX10.
4. Use `bpftrace` to hook `amdgpu_cs_ioctl` and count submissions per second under a benchmark:
   ```
   bpftrace -e 'kprobe:amdgpu_cs_ioctl { @ = count(); } interval:s:1 { print(@); clear(@); }'
   ```
5. Read `Documentation/gpu/amdgpu/cs.rst` (if present) or follow a CS through `amdgpu_trace.h` tracepoints.

---

Next: [Chapter 50 — The GPU scheduler and dma-fence](50-scheduler-and-fences.md).
