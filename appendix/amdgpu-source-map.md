# Appendix — A guided tour of `drivers/gpu/drm/amd/amdgpu/`

This is the file you open when you start at AMD and your manager says "have a look at the driver." Don't open the directory and panic at 800 files. Open these, in this order.

The path prefix `drivers/gpu/drm/amd/amdgpu/` is omitted below.

## Tier 1 — read these first

| File | Why |
|---|---|
| `amdgpu_drv.c` | Module entry, `pci_driver` struct, the `pci_device_id` table, module params (`amdgpu.dpm=`, etc.). The "front door" of the driver. |
| `amdgpu.h` | Mega-header. `struct amdgpu_device` lives here. ~1500+ lines of typedefs and forward decls. Skim, don't memorize. |
| `amdgpu_device.c` | `amdgpu_device_init`, `amdgpu_device_fini`, IP block bring-up & tear-down sequence. The probe path lives here. |
| `amdgpu_kms.c` | DRM driver `open` / `postclose` hooks, ioctl table, `drm_driver` struct, fpriv allocation. |
| `amdgpu_ioctl.c` (split across files) | The amdgpu-specific ioctls live in `amdgpu_kms.c`, `amdgpu_cs.c`, `amdgpu_gem.c`, `amdgpu_ctx.c`. |

## Tier 2 — the resource managers

| File | Why |
|---|---|
| `amdgpu_object.{c,h}` | `amdgpu_bo` — wraps `ttm_buffer_object`, the BO type used everywhere. |
| `amdgpu_gem.c` | GEM ioctls (create, mmap, op, userptr) — the userspace BO API. |
| `amdgpu_ttm.c` | TTM glue — placement, eviction, blit-via-SDMA migration callbacks. |
| `amdgpu_vm.{c,h}` | Per-process GPUVM, page table walks, `amdgpu_bo_va`, mapping update path, eviction. The single most important file in the driver. |
| `amdgpu_vm_pt.c` | Page table allocation/update logic separated out. |
| `amdgpu_mn.c` | MMU notifiers for userptr (CPU page-table changes invalidate GPU mappings). |

## Tier 3 — command path

| File | Why |
|---|---|
| `amdgpu_cs.c` | The `AMDGPU_CS` ioctl. Validates the BO list, builds an `amdgpu_job`, hands to scheduler. The hot path. |
| `amdgpu_job.c` | `amdgpu_job` lifecycle, scheduler integration. |
| `amdgpu_ring.{c,h}` | Generic ring abstraction for GFX/SDMA/UVD/VCN/MES. |
| `amdgpu_ib.c` | Indirect Buffer helpers. |
| `amdgpu_fence.c` | dma-fence implementation backed by GPU sequence counters. Very small, very important. |
| `amdgpu_sync.c` | Aggregating multiple fences for a single dependency. |
| `amdgpu_ctx.c` | `amdgpu_ctx` (per-userspace-context) — owns per-context priorities, hangs counters. |

## Tier 4 — interrupt + reset

| File | Why |
|---|---|
| `amdgpu_irq.c` | IRQ source registration + dispatch table. |
| `amdgpu_ih.c` (and per-IP `vega20_ih.c`, `navi10_ih.c`, ...) | IH ring (interrupt aggregator). |
| `amdgpu_reset.c` | Reset logic, reset domain, links to `amdgpu_device.c::amdgpu_device_gpu_recover`. |
| `amdgpu_ras.c` | RAS (ECC) handling. |
| `amdgpu_ras_eeprom.c` | Bad page table persisted in EEPROM. |

## Tier 5 — display + per-IP

| Path | Why |
|---|---|
| `amdgpu/dc/` (under `drivers/gpu/drm/amd/display/`) | The Display Core, *huge*. Atomic state, link training, programming sequences. Treat as separate codebase. |
| `amdgpu_dm.c` (under `display/amdgpu_dm/`) | The bridge between DRM atomic API and DC. |
| `amdgpu/gfx_*.c`, `gmc_*.c`, `sdma_*.c`, `vcn_*.c`, ... | Per-IP block instantiations. Each file owns the silicon-specific tables. |
| `amdgpu/mes_*.c` | MES (Micro Engine Scheduler) firmware glue. |
| `amdgpu/psp_*.c` | Platform Security Processor — owns firmware loading. |
| `amdgpu/sdma_*.c` | SDMA queues used for buffer copy/migration. |

## Tier 6 — KFD (compute)

`drivers/gpu/drm/amd/amdkfd/` is its own driver path used by ROCm:

| File | Why |
|---|---|
| `kfd_module.c`, `kfd_device.c` | Bring-up. |
| `kfd_chardev.c` | The `/dev/kfd` ioctl table. |
| `kfd_process.c`, `kfd_process_queue_manager.c` | Per-process KFD state, queue management. |
| `kfd_svm.c` | Shared Virtual Memory. |

## How to navigate it

```bash
# 1. Use cscope or vim's tags + ripgrep.
cscope -Rb -s drivers/gpu/drm/amd
# 2. ALWAYS use git log for any function: who wrote it and why?
git log -p drivers/gpu/drm/amd/amdgpu/amdgpu_vm.c | less
# 3. Read commit messages of the file owners.
git log --format='%an' drivers/gpu/drm/amd/amdgpu/amdgpu_vm.c | sort | uniq -c | sort -rn | head
# 4. Check Documentation/gpu/amdgpu.rst.
```

## Suggested reading sequence (your first month)

| Week | Read |
|---|---|
| 1 | `amdgpu_drv.c`, `amdgpu_device.c::amdgpu_pci_probe()` end-to-end. |
| 2 | `amdgpu_kms.c` ioctl table, follow `AMDGPU_CS` into `amdgpu_cs.c`. |
| 3 | `amdgpu_vm.c` — read every function, draw the data structure on paper. |
| 4 | `amdgpu_fence.c` + `amdgpu_ring.c` + DRM scheduler `drivers/gpu/drm/scheduler/sched_main.c`. |

By the end you'll have something to say in any AMD interview question.
