# Chapter 48 — The amdgpu driver — top-level architecture

We've assembled all the prerequisites. Now we walk amdgpu from top to bottom.

## 48.1 Source map

```
drivers/gpu/drm/amd/amdgpu/
├── amdgpu.h                     omnibus header
├── amdgpu_drv.c                 module init, pci_driver, fops
├── amdgpu_device.c              probe, init/fini orchestration
├── amdgpu_kms.c                 ioctl entry points (open/info/etc.)
├── amdgpu_ttm.c                 TTM glue
├── amdgpu_object.c              BO create/destroy/move
├── amdgpu_gem.c                 GEM ioctls
├── amdgpu_cs.c                  command-submit ioctl  ← star of the show
├── amdgpu_ring.c                rings, IB submission
├── amdgpu_fence.c               HW seqno → dma_fence
├── amdgpu_sync.c                fence dependency tracking
├── amdgpu_vm.c                  per-process GPU page tables
├── amdgpu_irq.c                 IH ring drain, IRQ subsystem registration
├── amdgpu_ctx.c                 user-visible scheduling context
├── amdgpu_sched.c               DRM-sched glue
├── gfx_v*.c                     per-generation GFX IP
├── sdma_v*.c                    per-generation SDMA IP
├── vcn_v*.c                     video codec IP
├── jpeg_v*.c                    JPEG IP
├── mes_v*.c                     MES (microcontroller scheduler) IP
├── psp_v*.c                     PSP (firmware loader) IP
├── soc15.c, nbio_v*.c, gmc_v*.c …  per-IP HW shims
├── amdgpu_pm/                   power management
├── amdgpu_dm/, dc/              display core (separate Makefile)
└── amdgpu_kfd_*.c               KFD bridge (compute path)
```

`drivers/gpu/drm/amd/amdkfd/` — the KFD/HSA path (separate fd `/dev/kfd`).

`drivers/gpu/drm/amd/include/` — register definitions per ASIC.

## 48.2 The probe path

`amdgpu_pci_probe(pdev, id)` (in `amdgpu_drv.c`) — entry. High-level flow:

```c
adev = devm_drm_dev_alloc(&pdev->dev, &kms_driver, struct amdgpu_device, ddev);
adev->pdev = pdev;
amdgpu_driver_load_kms(adev, ent->driver_data);
drm_dev_register(&adev->ddev, ent->driver_data);
```

`amdgpu_driver_load_kms` calls `amdgpu_device_init` which does the heavy lifting:

1. Detect ASIC family / variant.
2. Map BARs (`pci_iomap`).
3. Init `amdgpu_irq`.
4. `amdgpu_atombios_init` (legacy, for older ASICs) — ATOM BIOS parsing.
5. Init `amdgpu_smu` — system management.
6. `psp_init` — load all per-IP firmware.
7. `amdgpu_device_ip_init`: walks every "IP block" and calls its `early_init`, then `sw_init` (alloc rings, fences), then `hw_init` (program HW).
8. `amdgpu_ttm_init` — set up VRAM and GTT memory managers.
9. `amdgpu_vm_init` — set up VM machinery.
10. `amdgpu_amdkfd_device_init` — KFD setup.
11. Register DRM device.

## 48.3 IP blocks

Each "IP" (Intellectual Property block) — GFX, SDMA, VCN, MES, PSP, GMC, SMU, DC — implements a vtable:

```c
struct amdgpu_ip_block_version {
    enum amd_ip_block_type type;
    u32 major, minor, rev;
    const struct amd_ip_funcs *funcs;
};

struct amd_ip_funcs {
    char *name;
    int (*early_init)(void *handle);
    int (*late_init)(void *handle);
    int (*sw_init)(void *handle);
    int (*sw_fini)(void *handle);
    int (*hw_init)(void *handle);
    int (*hw_fini)(void *handle);
    int (*suspend)(void *handle);
    int (*resume)(void *handle);
    bool (*is_idle)(void *handle);
    int (*wait_for_idle)(void *handle);
    int (*soft_reset)(void *handle);
    /* … */
};
```

`amdgpu_device.c::amdgpu_device_ip_init` walks the table and calls each in order. Suspend/resume walks in the right order. This is essentially OOP-in-C with each IP as a plugin.

To bring up a new ASIC, you (typically) port the IP block files to the new register addresses and add an entry. The driver's bring-up structure absorbs new generations cleanly.

## 48.4 The amdgpu_device struct

`struct amdgpu_device` is the **god-object** that holds everything. ~1000+ fields. Notable groups:

- `pdev`, `ddev` — PCI and DRM cores.
- `mmio[]`, `rio_mem`, `doorbell` — register banks.
- `firmware`, `psp`, `smu` — firmware/control state.
- `vm_manager` — GPUVM.
- `gmc` — GPU memory controller; describes VRAM/GTT regions.
- `gfx`, `sdma[]`, `mes`, `vcn[]`, `jpeg[]` — per-IP state.
- `irq` — IRQ subsystem.
- `mode_info` — KMS mode info.
- `dm`, `dc` — display core glue.
- `pm` — power management.
- `ras` — RAS (reliability, availability, serviceability).
- `ip_blocks[]` — the IP table.

A function rarely needs more than a few fields. But the struct is your map.

## 48.5 The IH (Interrupt Handler) pipeline

Hardware interrupts to amdgpu come through an **IH ring** — circular buffer in memory the CPU pre-allocates. When an event happens, hardware writes a 16-byte cookie into the ring and updates the write pointer.

```c
amdgpu_irq_handler(pci_irq) {
    /* read IH ring tail */
    while (head != tail) {
        process_one_cookie(ring[head]);
        head++;
    }
    schedule(tasklet);   /* deferred */
}
```

`process_one_cookie` dispatches to the registered IRQ source — vblank, ring-completion, page-fault, RAS, etc. Each `amdgpu_irq_src` has an `id_table` mapping (client_id, src_id) and a `process` callback.

This is the closest thing in the kernel to a publish/subscribe bus.

## 48.6 The CS path (preview)

`DRM_IOCTL_AMDGPU_CS` is the ioctl that submits work. We dedicate Chapter 49. High-level:

1. Userspace passes "chunks": IBs, deps (input fences), syncobjs.
2. Driver looks up BOs, takes ww_mutex on all (`ttm_eu_reserve_buffers`).
3. Validates IBs (security: no privileged register access, no out-of-bounds memory writes).
4. Builds a `drm_sched_job`.
5. Hands to DRM scheduler (Chapter 50).
6. Returns a fence handle to userspace.

## 48.7 The amdgpu_ring abstraction

Each engine has a ring (or several): GFX ring, compute rings, SDMA rings, VCN rings. The `struct amdgpu_ring`:

```c
struct amdgpu_ring {
    struct amdgpu_device  *adev;
    const struct amdgpu_ring_funcs *funcs;
    struct drm_gpu_scheduler  sched;
    struct amdgpu_bo  *ring_obj;
    volatile uint32_t  *ring;        /* CPU mapping of ring */
    unsigned   wptr, rptr;
    uint64_t   gpu_addr;
    /* … */
};
```

`funcs` is the per-ring vtable: how to write a "begin IB" packet, "fence" packet, etc. It abstracts PM4 vs. SDMA vs. VCN packet formats.

We discuss in Chapter 49.

## 48.8 The fence path

amdgpu's HW writes a sequence number to a memory address when each job finishes. The driver:

- Allocates a `dma_fence` for each submission.
- Polls (and registers an interrupt source) for sequence number progression.
- Signals the fence when the seqno passes.

Code: `amdgpu_fence_emit`, `amdgpu_fence_process`. We discuss in Chapter 50.

## 48.9 GPUVM

Each open file (or KFD process) has its own GPU virtual address space. Page tables live in VRAM (or system memory). Levels: PD0 → PD1 → PD2 → PT.

When userspace `MAP`s a BO, the driver writes PTEs. When the BO migrates (TTM moves), PTEs are updated. When a BO is freed, PTEs are zeroed.

Updates can go via SDMA copy (asynchronously, very fast for batches) or CPU writes. amdgpu uses both depending on circumstance.

## 48.10 RAS, reset, recovery

When the GPU hangs (job timeout) or a hardware error fires, amdgpu issues a **reset**. Two flavors:

- **Soft reset** — reset specific IPs, reload firmware, recover.
- **Mode-1 / Mode-2 / FLR** — bigger hammers, up to PCI Function-Level Reset.

After reset, all in-flight jobs are killed (their fences signaled with error), VM PTEs may need rebuilding, displays must be re-enabled. amdgpu does all of this without losing the desktop.

`amdgpu_device.c::amdgpu_device_gpu_recover` is where it happens. Massive function. Worth reading once you've absorbed the rest.

## 48.11 Reading the source: a recommended path

1. `amdgpu_drv.c` — module init, ioctls table.
2. `amdgpu_device.c` — probe.
3. `amdgpu_kms.c` — open/close.
4. `amdgpu_gem.c` — BO create/free/mmap.
5. `amdgpu_ttm.c` — TTM glue.
6. `amdgpu_cs.c` — command submission.
7. `amdgpu_ring.c`, `amdgpu_ib.c` — IB push.
8. `amdgpu_fence.c` — fences.
9. `amdgpu_vm.c` — VM (long).
10. `amdgpu_irq.c` — IH ring.

Then dive into one IP (say, `gfx_v10_0.c`) to see how a generation handles GFX init.

## 48.12 Exercises

1. Boot with `amdgpu.debug_mask=0xfff` and capture the dmesg from `amdgpu` load. Identify the IP-block sequence.
2. Read `amdgpu_drv.c::amdgpu_pci_probe`. Take notes on the 50 lines.
3. Read `amdgpu_device.c::amdgpu_device_ip_init`. Spot the call to each IP's `sw_init` then `hw_init`.
4. Read `amdgpu_ring.c::amdgpu_ring_init`. Note the BO allocation for the ring buffer + fences.
5. Run `cat /sys/kernel/debug/dri/0/amdgpu_firmware_info`. See every firmware version loaded.

---

Next: [Chapter 49 — Command submission, IBs, rings, doorbells](49-command-submission.md).
