# Chapter 44 — DRM, the kernel's GPU framework

DRM (Direct Rendering Manager) is the kernel subsystem every Linux GPU driver lives in. amdgpu, i915, nouveau, msm, etnaviv, lima, panfrost — all are DRM drivers. This chapter lays out DRM's structure and the boilerplate every driver writes.

## 44.1 What DRM provides

DRM provides:

- A **char device** (`/dev/dri/cardN`, `/dev/dri/renderDN`).
- A **standard ioctl set** (open/close, GEM, syncobj, prime, KMS).
- **Per-driver ioctls** registered as a vendor table (the AMDGPU_* ioctls).
- **GEM** — a generic buffer-object framework.
- **Atomic KMS** — modesetting.
- **DRM scheduler** — generic GPU job scheduler used by amdgpu.
- **PRIME / dma-buf** — cross-driver buffer sharing.
- **DRM master** — exclusive control of display.
- **GEM helpers**, **GEM SHMEM**, **GEM CMA** — buffer-management helpers for simpler drivers.

The core lives in `drivers/gpu/drm/`. amdgpu under `amd/amdgpu/`.

## 44.2 Where DRM glues in

```
drivers/gpu/drm/drm_*.c              ← DRM core
drivers/gpu/drm/scheduler/*.c        ← DRM scheduler
drivers/gpu/drm/ttm/*.c              ← TTM (used by amdgpu)
drivers/gpu/drm/amd/amdgpu/*.c       ← amdgpu IPs
drivers/gpu/drm/amd/display/*        ← DC display
drivers/gpu/drm/amd/include/*        ← register defs
include/drm/*.h                       ← DRM API headers
```

## 44.3 The driver-author's deliverables

A DRM driver provides at minimum:

1. A `struct drm_driver` (the vtable for everything).
2. An `unsigned long driver_features` mask (`DRIVER_GEM`, `DRIVER_RENDER`, `DRIVER_MODESET`, `DRIVER_ATOMIC`, …).
3. Per-driver ioctls: `struct drm_ioctl_desc ioctls[]`.
4. `file_operations` glue (mostly stock helpers).
5. `drm_device` allocation (`drm_dev_alloc`) and registration (`drm_dev_register`).
6. Optional: KMS implementation, BO management.

amdgpu's `struct drm_driver`:

```c
static struct drm_driver kms_driver = {
    .driver_features  = DRIVER_ATOMIC | DRIVER_GEM | DRIVER_RENDER
                      | DRIVER_MODESET | DRIVER_SYNCOBJ
                      | DRIVER_SYNCOBJ_TIMELINE,
    .open             = amdgpu_driver_open_kms,
    .postclose        = amdgpu_driver_postclose_kms,
    .lastclose        = amdgpu_driver_lastclose_kms,
    .ioctls           = amdgpu_ioctls_kms,
    .num_ioctls       = ARRAY_SIZE(amdgpu_ioctls_kms),
    .fops             = &amdgpu_driver_kms_fops,
    .gem_prime_import = amdgpu_gem_prime_import,
    /* … */
};
```

We'll see this struct repeatedly.

## 44.4 DRM file_operations

`amdgpu_driver_kms_fops` is mostly stock helpers:

```c
static const struct file_operations amdgpu_driver_kms_fops = {
    .owner    = THIS_MODULE,
    .open     = drm_open,
    .release  = drm_release,
    .unlocked_ioctl = amdgpu_drm_ioctl,
    .mmap     = drm_gem_mmap,
    .poll     = drm_poll,
    .read     = drm_read,
    .compat_ioctl = amdgpu_kms_compat_ioctl,
    .llseek   = noop_llseek,
};
```

`drm_open`, `drm_release` — DRM core's open/close. They allocate the per-fd `struct drm_file`, hook into authentication, and call `drv->open` when present.

## 44.5 The ioctl table

```c
static const struct drm_ioctl_desc amdgpu_ioctls_kms[] = {
    DRM_IOCTL_DEF_DRV(AMDGPU_GEM_CREATE, amdgpu_gem_create_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_CTX,        amdgpu_ctx_ioctl,        DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_BO_LIST,    amdgpu_bo_list_ioctl,    DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_CS,         amdgpu_cs_ioctl,         DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_INFO,       amdgpu_info_ioctl,       DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_GEM_METADATA,amdgpu_gem_metadata_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_GEM_VA,     amdgpu_gem_va_ioctl,     DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_GEM_OP,     amdgpu_gem_op_ioctl,     DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_GEM_USERPTR,amdgpu_gem_userptr_ioctl,DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_WAIT_CS,    amdgpu_cs_wait_ioctl,    DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_WAIT_FENCES,amdgpu_cs_wait_fences_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(AMDGPU_SCHED,      amdgpu_sched_ioctl,      DRM_MASTER),
    DRM_IOCTL_DEF_DRV(AMDGPU_VM,         amdgpu_vm_ioctl,         DRM_AUTH | DRM_RENDER_ALLOW),
    /* … */
};
```

Each entry points at a function and declares permissions. `DRM_RENDER_ALLOW` lets the ioctl be used on `renderD*` (no DRM-master required).

DRM core handles the copy_from/to_user, validates struct sizes, and dispatches.

## 44.6 The `drm_device`

`struct drm_device` is DRM's device handle. Allocated with `drm_dev_alloc()`, registered with `drm_dev_register()`. Embedded inside a driver-specific struct (e.g. `struct amdgpu_device`):

```c
struct amdgpu_device {
    struct drm_device         ddev;     /* embedded */
    struct pci_dev           *pdev;
    /* … hundreds more fields … */
};
```

`container_of(ddev, struct amdgpu_device, ddev)` recovers the parent.

## 44.7 The `drm_file`

Per-open file. Points to `drm_device`. Has `driver_priv` for the driver's per-fd state (`struct amdgpu_fpriv`).

`amdgpu_fpriv` holds:

- a per-process VM (GPUVM),
- a context array (`amdgpu_ctx_mgr`),
- BO list table,
- syncobj table.

Multiple opens by one process can have multiple file pointers; usually one process opens once.

## 44.8 Authentication and master

For KMS ioctls, DRM master controls the display. Only one process at a time. X11 server, Wayland compositor become master.

Render-only clients (Vulkan apps, ROCm runtime) don't need master — they use `renderD*`.

In `amdgpu_ioctl`, DRM core checks the flags. Drivers don't re-check.

## 44.9 dma-buf / PRIME

DRM's dma-buf integration goes by the name **PRIME**. Two ioctls:

- `DRM_IOCTL_PRIME_HANDLE_TO_FD` — turn a GEM handle into a dma-buf fd.
- `DRM_IOCTL_PRIME_FD_TO_HANDLE` — import a dma-buf fd as a GEM handle.

Used for hand-off between two GPUs (hybrid graphics, prime offload), or between GPU and codec/display.

amdgpu provides `gem_prime_import` / `gem_prime_export` callbacks that work with TTM's dma-buf integration.

## 44.10 KMS at a glance (full chapter later)

For display:

- **Connector**: physical port (HDMI, DP, eDP).
- **Encoder**: signal generator (TMDS, DP).
- **CRTC**: frame timing generator + scanout pipe.
- **Plane**: image source layered onto a CRTC (primary, cursor, overlay).
- **Framebuffer**: GEM BO with format/pitch metadata.

Atomic modeset: state is a struct; you build a desired state; commit atomically. Either everything goes or nothing.

amdgpu DC implements all of this.

## 44.11 The DRM scheduler (full chapter later)

`drivers/gpu/drm/scheduler/` provides per-hardware-queue scheduling: takes `drm_sched_job`s, respects input fences, runs them in order, signals output fences. Each amdgpu engine ring has a `drm_gpu_scheduler`.

We treat in Chapter 50.

## 44.12 The minimal DRM driver

For comparison, `vkms` (Virtual KMS) in `drivers/gpu/drm/vkms/` is ~1500 lines and a complete DRM driver. Read it; it is a wonderful exemplar.

```c
static struct drm_driver vkms_driver = {
    .driver_features = DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_GEM,
    .open            = vkms_postclose,
    .fops            = &vkms_driver_fops,
    .gem_prime_import_sg_table = vkms_prime_import_sg_table,
    DRM_GEM_SHMEM_DRIVER_OPS,
    .name            = "vkms",
    /* … */
};
```

`DRM_GEM_SHMEM_DRIVER_OPS` is a macro that fills in the GEM SHMEM helpers. amdgpu uses TTM instead, which is the heavier-weight path.

## 44.13 Exercises

1. Read the entire `vkms` driver (4–6 files). Identify how it implements every required hook.
2. Read `include/drm/drm_drv.h::struct drm_driver`. Understand each field.
3. Read `drivers/gpu/drm/drm_drv.c::drm_dev_alloc`, `drm_dev_register`. Note refcounting.
4. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c::amdgpu_ioctls_kms`. Identify all user-visible ioctls.
5. Open `/dev/dri/renderD128` from a userspace program; call `DRM_IOCTL_VERSION` to get the driver name and major/minor.

---

Next: [Chapter 45 — GEM — buffer objects](45-gem.md).
