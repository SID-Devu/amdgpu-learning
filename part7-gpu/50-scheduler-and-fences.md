# Chapter 50 — The GPU scheduler and dma-fence

The DRM scheduler (`drivers/gpu/drm/scheduler/`) and the kernel's `dma_fence` are the dual pillars of GPU concurrency in Linux. Every Vulkan timeline semaphore, every Wayland surface flip, every cross-process buffer hand-off lives on top of these. Read this chapter twice.

## 50.1 The DRM scheduler

A `struct drm_gpu_scheduler` is created per **hardware ring**. It has:

- a list of `drm_sched_entity` (one per app context per ring),
- a worker thread that pulls jobs and runs them,
- timeout machinery for hung jobs.

```c
struct drm_gpu_scheduler {
    const struct drm_sched_backend_ops *ops;
    uint32_t                            hw_submission_limit;
    long                                timeout;
    const char                         *name;
    struct list_head                    sched_rq[DRM_SCHED_PRIORITY_COUNT];
    wait_queue_head_t                   job_scheduled;
    atomic_t                            hw_rq_count;
    struct task_struct                 *thread;
    /* … */
};
```

Backend ops the driver implements:

```c
struct drm_sched_backend_ops {
    struct dma_fence *(*prepare_job)(struct drm_sched_job *, struct drm_sched_entity *);
    struct dma_fence *(*run_job)(struct drm_sched_job *sched_job);
    enum drm_gpu_sched_stat (*timedout_job)(struct drm_sched_job *sched_job);
    void (*free_job)(struct drm_sched_job *sched_job);
};
```

amdgpu's implementations: `amdgpu_job_run`, `amdgpu_job_timedout`, `amdgpu_job_free`. We saw `run_job` in the previous chapter.

## 50.2 Entities

A `drm_sched_entity` represents "one stream of work from one client." A single application context typically has one entity per ring it uses (one GFX entity, one compute entity, etc.).

```c
struct drm_sched_entity {
    struct list_head    list;
    struct drm_sched_rq *rq;
    struct drm_gpu_scheduler **sched_list;
    /* … */
    struct spsc_queue   job_queue;
    struct dma_fence  *last_scheduled;
    /* … */
};
```

`spsc_queue` is a *single-producer single-consumer* lock-free FIFO. The producer is your `amdgpu_cs_ioctl` thread; the consumer is the scheduler's worker. The same SPSC pattern from Chapter 23.

## 50.3 The lifecycle of a job

```c
amdgpu_job_alloc();                      /* allocate */
drm_sched_job_init(&job->base, entity);  /* attach to entity */
drm_sched_job_arm(&job->base);           /* generate fence */
job->base.s_fence ← new dma_fence

drm_sched_entity_push_job(&job->base);   /* into entity SPSC queue */
   ▼
scheduler thread loops:
    job = drm_sched_entity_pop_job(entity);
    /* wait on job->dependencies */
    fence = ops->run_job(job);
    /* attach `finish_cb` to `fence` so when HW fence signals, sched fence does too */
   ▼
HW fence signals on completion (IRQ → amdgpu_fence_process)
   ▼
sched fence is signaled
   ▼
ops->free_job(job)
```

The scheduler's *sched fence* (`s_fence`) has two parts: a **scheduled** fence (signaled when the job has been pushed to HW) and a **finished** fence (signaled when HW completes). Userspace usually waits on `finished`.

## 50.4 Dependencies

Each job has a list of input fences. `drm_sched` waits on all of them before calling `run_job`. Dependencies come from:

- Same context, prior IB on a ring (sequencing).
- Cross-engine dependencies (compute → GFX).
- Cross-process syncobj signals.
- BO resv lists (writer fence on a BO must complete before next reader).

amdgpu's `amdgpu_cs_dependencies` builds this fence list during CS parsing.

## 50.5 dma-fence

```c
struct dma_fence {
    spinlock_t           *lock;
    const struct dma_fence_ops *ops;
    struct rcu_head       rcu;
    struct list_head      cb_list;
    union {
        struct list_head  cb_list;
        ktime_t           timestamp;
    };
    u64                   context;
    u64                   seqno;
    unsigned long         flags;
    struct kref           refcount;
    int                   error;
};
```

State machine:

- Created: not signaled.
- Enable signaling: driver arms hardware notification.
- Signaled: `flags |= SIGNALED`. Wakers run.

Operations:

- `dma_fence_get`/`put` — refcount.
- `dma_fence_signal()` — driver signals from IRQ or wherever.
- `dma_fence_add_callback(f, cb, fn)` — async callback.
- `dma_fence_wait[_timeout]()` — sync wait.
- `dma_fence_default_wait()` — kernel default.

`dma_fence_context_alloc()` reserves a unique 64-bit context ID. Within a context, sequence numbers must be monotonically increasing.

## 50.6 amdgpu fences

Each amdgpu ring has a per-ring dma-fence sequence:

```c
struct amdgpu_fence_driver {
    uint32_t                  num_fences_mask;
    spinlock_t                lock;
    struct dma_fence        **fences;     /* circular buffer of pending */
    uint64_t                  sync_seq;
    atomic_t                  last_seq;   /* latest signaled */
    bool                      initialized;
    /* … */
};
```

When a CS submits, the driver picks the next seqno, embeds a `RELEASE_MEM` packet that writes that seqno to a known memory location with cache flush. When `amdgpu_fence_process()` runs (from IRQ or polling), it reads `last_seq` from the memory and signals all dma-fences up to that point.

## 50.7 Timeouts and reset

If a job doesn't finish within its timeout (`drm_gpu_scheduler.timeout`, default 10 s for amdgpu), `timedout_job()` is called. amdgpu:

1. Logs the hang (dmesg + debugfs).
2. Triggers a GPU reset (`amdgpu_device_gpu_recover`).
3. Marks the job and its fence with `-ECANCELED`.
4. After reset, scheduler resumes; subsequent jobs run.

This is a critical reliability property: a single bad shader doesn't take down the whole desktop.

## 50.8 Priority and preemption

Each entity has a priority. Higher priority preempts lower at the queue level (the scheduler picks high-prio first when both are ready). For *true* hardware preemption (mid-job interruption), the GPU's CP must support it; modern amdgpu uses MES for compute preemption.

User-visible priority via `DRM_IOCTL_AMDGPU_SCHED` — set context priority. `niceness` for graphics under PCs is rarely changed; for compute servers, it's important.

## 50.9 Syncobjs and timeline semaphores

`struct drm_syncobj` wraps a single dma-fence (binary syncobj) or a timeline of fences (timeline syncobj). It exposes a userspace handle and supports:

- Waiting for a fence to signal.
- Importing/exporting via fd.
- Timeline `wait for value ≥ N`.

amdgpu plugs syncobj into the CS path: `chunk SYNCOBJ_IN` adds dependencies; `chunk SYNCOBJ_OUT` materializes the output fence.

## 50.10 Locking model

The scheduler thread holds:

- `entity->lock` while popping the job.
- No global scheduler lock during `run_job`.
- `s_fence->lock` (spinlock) when signaling.

Driver's `run_job` runs in the scheduler kthread context. It can sleep, take mutexes, do DMA, but it's the hot path — keep it fast.

`free_job` runs from the scheduler thread or from the fence callback context.

## 50.11 amdgpu jobs in code

```c
struct amdgpu_job {
    struct drm_sched_job  base;
    struct amdgpu_vm     *vm;
    struct amdgpu_ring   *ring;
    struct amdgpu_sync    sync;
    uint32_t              gds_base, gds_size, gws_base, gws_size, oa_base, oa_size;
    uint32_t              vmid;
    uint64_t              uf_addr;
    bool                  is_killed;
    /* … */
    struct amdgpu_ib      ibs[];
};
```

`run_job` (= `amdgpu_job_run`) does:

1. VM switch (program VMID).
2. Emit per-engine prologue.
3. For each IB, push `INDIRECT_BUFFER` packet.
4. Emit RELEASE_MEM (the fence).
5. Commit ring.
6. Return the fence.

## 50.12 Exercises

1. Read `drivers/gpu/drm/scheduler/sched_main.c::drm_sched_main`.
2. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_job.c`.
3. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_fence.c::amdgpu_fence_emit` and `_process`.
4. Read `include/linux/dma-fence.h`. Memorize the ops table.
5. Use bpftrace on `amdgpu_fence_emit` and `amdgpu_fence_signal`; count rates under a load. Should be roughly equal at steady state.

---

Next: [Chapter 51 — VM, page tables, and unified memory](51-gpu-vm.md).
