# Chapter 43 — The Linux graphics stack from app to silicon

Before we dive into the kernel side, we need to know what's on top: how a Vulkan call reaches amdgpu, what Mesa is, what libdrm is, what KMS is. Then the kernel work makes sense.

## 43.1 The big picture

```
┌────────────────────────────────────────────────────────────┐
│   Application: glmark2, vkcube, dav1d, blender, hipify-app │
└─────────────────────┬──────────────────────────────────────┘
              OpenGL / Vulkan / OpenCL / HIP
                      │
┌─────────────────────▼──────────────────────────────────────┐
│   API loader: libGL.so / libvulkan.so / libamdhip64.so     │
└─────────────────────┬──────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────┐
│   Vendor userspace driver:                                 │
│     - Mesa radeonsi  (OpenGL, GLES)                        │
│     - Mesa radv      (Vulkan)                              │
│     - AMDVLK         (AMD's open Vulkan)                   │
│     - ROCclr / ROCr  (HIP, OpenCL ROCm)                    │
│     - PAL / DXC      (proprietary on Windows)              │
│   builds command buffers, manages BOs, submits via libdrm  │
└─────────────────────┬──────────────────────────────────────┘
                      │ libdrm_amdgpu.so
┌─────────────────────▼──────────────────────────────────────┐
│   /dev/dri/renderDxxx  (or /dev/kfd)                       │
└─────────────────────┬──────────────────────────────────────┘
              syscalls (ioctl, mmap, …)
┌─────────────────────▼──────────────────────────────────────┐
│   amdgpu.ko (kernel)                                        │
│      DRM core, GEM, TTM, DRM-Sched, KMS, PM, IRQ            │
└─────────────────────┬──────────────────────────────────────┘
                      │ MMIO + DMA + doorbells
┌─────────────────────▼──────────────────────────────────────┐
│   GPU silicon                                              │
└────────────────────────────────────────────────────────────┘
```

You will work in the `amdgpu.ko` row, but understanding the rows above lets you debug from "Vulkan game flickers" to "dma-buf import fails."

## 43.2 The userspace driver's job

Given a Vulkan command list:

1. **Compile shaders** — the driver embeds an LLVM-backed compiler producing AMDGPU ISA.
2. **Allocate buffers** — vertex, index, descriptor, image, uniform. Each becomes a BO via `DRM_IOCTL_AMDGPU_GEM_CREATE`.
3. **Build a PM4 / SDMA stream** in CPU-mapped memory.
4. **Submit** via `DRM_IOCTL_AMDGPU_CS` with the IB and a list of BOs ("BO list") and dependency fences.
5. **Wait** on the returned fence (`DRM_IOCTL_AMDGPU_CS` returns a sequence number; userspace can poll or `DRM_IOCTL_AMDGPU_WAIT_FENCES`).

amdgpu's userspace partner, `libdrm_amdgpu`, wraps these ioctls in a clean C API.

## 43.3 The display path: DRM/KMS

For displaying a rendered frame:

1. Userspace allocates a "scanout-able" BO.
2. Renders into it.
3. Calls `DRM_IOCTL_MODE_CRTC_PAGE_FLIP` (or atomic: `DRM_IOCTL_MODE_ATOMIC`) to flip the active framebuffer.
4. Kernel programs the display engine.

This is the **KMS** (Kernel Mode Setting) part of DRM. amdgpu's display piece is **DC** (Display Core) — a huge separate codebase under `drivers/gpu/drm/amd/display/`.

For headless / compute-only systems, KMS is not used; userspace just opens `/dev/dri/renderD*`.

## 43.4 `/dev/dri/cardN` vs `/dev/dri/renderDN`

- **cardN** — primary node. Has KMS ioctls. Requires `DRM_AUTH` for sensitive things.
- **renderDN** — render-only node. No KMS, no auth. Cheaper, used by compute and many Vulkan render-only paths.

For compute-only scenarios, `renderD*` is the entry. ROCm primarily uses `renderD*` and `/dev/kfd`.

## 43.5 Key userspace bits

- **Mesa**: open-source GL/Vulkan/OpenCL stack. `radeonsi` (GL), `radv` (Vulkan), `clover`/`rusticl` (OpenCL).
- **AMDVLK**: AMD's official Vulkan driver, also open-source.
- **libdrm**: thin wrapper over DRM ioctls. `libdrm_amdgpu` is its amdgpu-specific subset.
- **wlroots / Mutter / Sway / KWin**: Wayland compositors using KMS+DRM directly.
- **ROCm runtime (ROCr/HSA)**: `librocm-hsa-runtime64.so`. Talks to `/dev/kfd`.
- **HIP runtime**: `libamdhip64.so`. Built on ROCr.

## 43.6 Patches typically span layers

A single feature (say, a new compression mode) may require:

- **HW spec / ISA** updated (silicon team).
- **kernel** ioctl tag and new packet generation in amdgpu.
- **libdrm_amdgpu** API addition.
- **Mesa radv** updated to use the new mode.
- **Vulkan tests** in `vkcts` updated.

Working on one layer demands at least a reading-knowledge of the rest. Pick a feature you care about and follow its path through all layers — that's the fastest way to develop perspective.

## 43.7 Wayland and X11 differences (briefly)

- **X11**: monolithic server. DRI3 lets clients render to dma-bufs, server scans them out.
- **Wayland**: compositor is the server; clients hand dma-bufs to it. Direct, less latency. Linux is migrating to Wayland.

For driver work, both look the same: a process opens `/dev/dri/cardN`, becomes DRM master, programs KMS. Below KMS the driver doesn't see who the client was.

## 43.8 ROCm path

For HIP/ROCm:

```
HIP app → libamdhip64.so → libhsa-runtime64.so → /dev/kfd → amdkfd/amdgpu
```

The KFD path bypasses graphics: it allocates BOs through KFD ioctls, registers user-mode queues with MES, and userspace writes commands directly into shared queues without per-submit ioctl. Doorbells trigger the MES to run them.

This drives the very low submission overhead ROCm needs.

## 43.9 Putting your first userspace prog together

A "hello GPU" with libdrm_amdgpu (we'll write fully in Chapter 52):

```c
amdgpu_device_handle dev;
amdgpu_device_initialize(fd, &major, &minor, &dev);

amdgpu_bo_handle bo;
amdgpu_bo_alloc_request req = { .alloc_size = 4096, .preferred_heap = AMDGPU_GEM_DOMAIN_GTT };
amdgpu_bo_alloc(dev, &req, &bo);

amdgpu_bo_cpu_map(bo, &cpu_addr);
((uint32_t*)cpu_addr)[0] = 0xCAFEBABE;
amdgpu_bo_cpu_unmap(bo);

amdgpu_bo_free(bo);
amdgpu_device_deinitialize(dev);
```

Less than 30 lines and you're talking to the GPU. We'll extend this to actually submit a command stream.

## 43.10 Exercises

1. `vulkaninfo` (install `vulkan-tools`). Read every line. Note the device name, queues, memory heaps.
2. `glxinfo | grep OpenGL` — the renderer string from Mesa.
3. `ls /dev/dri` — see your nodes. Read `/sys/class/drm/renderD*/device/driver`.
4. `strace -f -e trace=ioctl glxgears 2>&1 | head -200` — observe the DRM ioctls.
5. Read `Documentation/gpu/drm-uapi.rst`.

---

Next: [Chapter 44 — DRM, the kernel's GPU framework](44-drm-core.md).
