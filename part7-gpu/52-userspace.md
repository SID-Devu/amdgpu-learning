# Chapter 52 — Userspace: libdrm, Mesa, ROCm, KFD

A kernel driver that no userspace can talk to is dead. AMD's userspace stack has many layers; you'll need to navigate them when debugging anything visible.

## 52.1 libdrm and libdrm_amdgpu

`libdrm.so` is the thin DRM ioctl wrapper. `libdrm_amdgpu.so.1` is its amdgpu-specific subset; ships with the system.

Public headers: `xf86drm.h`, `xf86drmMode.h`, `amdgpu.h`, `amdgpu_drm.h`.

A minimal program:

```c
#include <fcntl.h>
#include <stdio.h>
#include <amdgpu.h>
#include <amdgpu_drm.h>

int main(void) {
    int fd = open("/dev/dri/renderD128", O_RDWR);
    amdgpu_device_handle dev;
    uint32_t major, minor;
    amdgpu_device_initialize(fd, &major, &minor, &dev);

    struct amdgpu_gpu_info info;
    amdgpu_query_gpu_info(dev, &info);
    printf("family %u  asic_id 0x%x  pci %04x:%04x num_shader_engines %u\n",
           info.family_id, info.asic_id, info.vendor_id, info.device_id,
           info.num_shader_engines);

    amdgpu_device_deinitialize(dev);
    close(fd);
    return 0;
}
```

Build:

```bash
gcc t.c -ldrm_amdgpu -o t -I/usr/include/libdrm
```

You're talking to amdgpu without Mesa. This is what we'll extend in the labs.

## 52.2 Mesa

Mesa is the open-source graphics stack. For AMD:

- `src/gallium/drivers/radeonsi` — OpenGL.
- `src/amd/vulkan` (`radv`) — Vulkan.
- `src/amd/compiler` (`aco`) — shader compiler (besides LLVM AMDGPU).
- `src/amd/common` — shared utilities.
- `src/amd/llvm` — LLVM-backed compile path.

Build & install:

```bash
git clone https://gitlab.freedesktop.org/mesa/mesa.git
cd mesa
meson setup build -Dvulkan-drivers=amd -Dgallium-drivers=radeonsi
ninja -C build
sudo ninja -C build install
```

Test with `vkcube` (Vulkan) or `glmark2` (OpenGL).

## 52.3 ROCm runtime

ROCr / HSA runtime sits below HIP. For our purposes:

- `/dev/kfd` — KFD device.
- `librocm-hsa-runtime64.so` — talks to KFD.
- `libamdhip64.so` — HIP layer over ROCr.

KFD adds ioctls beyond DRM:

- `KFD_IOC_CREATE_QUEUE` — user-mode queue.
- `KFD_IOC_DESTROY_QUEUE`.
- `KFD_IOC_SET_MEMORY_POLICY`.
- `KFD_IOC_ALLOC_MEMORY_OF_GPU`, `_FREE`.
- `KFD_IOC_MAP_MEMORY_TO_GPU`, `_UNMAP`.
- `KFD_IOC_SVM`.
- … (~70 ioctls in modern KFD).

When you write HIP, every `hipMalloc`/`hipMemcpy`/`hipLaunchKernel` ultimately hits KFD ioctls.

## 52.4 Reading HIP/ROCm source

Useful repos (all on github.com/ROCm):

- `ROCR-Runtime` — HSA runtime.
- `clr` (formerly ROCclr / HIP clr) — ROCm common language runtime.
- `HIP` — HIP API.
- `ROCm-CompilerSupport`, `llvm-project` — compilers.
- `ROCm-Device-Libs`, `aomp` — device libs.
- `MIOpen`, `rocBLAS`, `RCCL` — math libraries.

The kernel-side counterpart is amdkfd; the userspace counterpart is HSA runtime.

## 52.5 A minimal compute submission via libdrm_amdgpu

Skeleton (full code in labs):

```c
amdgpu_device_initialize(fd, &maj, &min, &dev);

amdgpu_context_handle ctx;
amdgpu_cs_ctx_create(dev, &ctx);

/* allocate IB BO */
amdgpu_bo_alloc_request req = { .alloc_size = 4096,
                                 .preferred_heap = AMDGPU_GEM_DOMAIN_GTT };
amdgpu_bo_handle ib_bo;
amdgpu_bo_alloc(dev, &req, &ib_bo);

uint64_t ib_va = ...; /* via amdgpu_va */
uint32_t *ib;
amdgpu_bo_cpu_map(ib_bo, (void**)&ib);

/* fill IB with PM4 packets: e.g. write 0xcafebabe to a known BO address */

amdgpu_cs_request request = { .ip_type = AMDGPU_HW_IP_GFX,
                              .number_of_ibs = 1,
                              .ibs = &(struct amdgpu_cs_ib_info){
                                  .ib_mc_address = ib_va,
                                  .size = ib_dw_count } };
uint64_t seq;
amdgpu_cs_submit(ctx, 0, &request, 1);
amdgpu_cs_query_fence_status(...);    /* wait for completion */
```

A lab exercise will turn this skeleton into a complete runnable example.

## 52.6 The Vulkan layer at AMD

`radv` is the dominant open-source Vulkan driver for AMD on Linux. AMDVLK is AMD's official open-source. Both submit through libdrm_amdgpu. `radv` uses the `aco` compiler which is faster to compile shaders than LLVM.

For your job: read `src/amd/vulkan/radv_cmd_buffer.c`. It builds command buffers identical in spirit to what we did with libdrm_amdgpu — much more elaborate.

## 52.7 The X11/Wayland integration

- **X11 / DRI3**: clients render to dma-bufs, X server (Xorg with xf86-video-amdgpu, or Xwayland) scans them out.
- **Wayland**: compositor uses libdrm directly; calls KMS atomic to flip; clients send dma-bufs over the wire.

For driver work, both look the same — it's all atomic + GEM + dma-buf below.

## 52.8 Tools you should have

- `glxinfo`, `vulkaninfo`, `clinfo` — capability dumps.
- `vkcube`, `glmark2`, `glxgears` — smoke tests.
- `mangohud` — overlay; FPS, GPU utilization.
- `rocm-smi`, `rocminfo`, `rocprof` — ROCm-side.
- `radeontop` — utilization per ring.
- `umr` (UMR-debugger) — register dump tool, official for amdgpu.

## 52.9 Debugging userspace ↔ kernel

A bug that "starts in Mesa" sometimes ends in amdgpu. Triage:

1. Reproduce with the simplest test (`vkcube`, `glxgears`).
2. `strace -e ioctl` — what ioctls?
3. `dmesg` — any warnings?
4. `cat /sys/kernel/debug/dri/0/amdgpu_*` — relevant state?
5. `rocm-smi` — GPU temp/fan/voltage.
6. If hang: `umr` to dump registers; `cat /sys/kernel/debug/dri/0/amdgpu_gpu_recover` to force reset.
7. If still mysterious: `ftrace amdgpu*` and `bpftrace`.

Never assume a bug is in one layer until you've ruled out the others.

## 52.10 Exercises

1. Build the skeleton from §52.5; submit a "WRITE_DATA" PM4 packet that writes a value to a BO; read back; verify.
2. Build a Vulkan minimal compute example with `radv`; observe ioctls with strace.
3. Build Mesa from source against your kernel headers. Run `vulkaninfo` with the new driver.
4. Read one ioctl entry-to-completion: `vkQueueSubmit` → `radv_queue_submit_normal` → `amdgpu_cs_submit_*` → kernel `amdgpu_cs_ioctl`.
5. Use `mangohud` while running a game; correlate FPS dips with `dmesg` events.

---

Next: [Chapter 53 — Reset, recovery, RAS, and the real world](53-reset-and-recovery.md).
