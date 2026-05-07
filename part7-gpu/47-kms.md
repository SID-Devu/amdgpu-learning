# Chapter 47 — KMS — display, planes, CRTCs, atomic modesetting

KMS (Kernel Mode Setting) lives in DRM and is how Linux puts pixels on the screen. amdgpu's display side is its own giant subsystem (DC), but it sits behind a standard KMS API that we walk here. If you join the AMD display team you'll be inside DC; otherwise this chapter is enough to debug display issues from the rest of the driver.

## 47.1 The objects

```
   Connector  →  Encoder  →  CRTC  →  [Plane(s)] → Framebuffer (BO)
   "the port"   "signals"    "timer"   "image"     "memory"
```

- **Connector**: a physical port (HDMI, DP, eDP, VGA). Has detection, EDID, supported modes.
- **Encoder**: the signal generator (TMDS for HDMI, DisplayPort transport, etc.). 1:1 with a connector usually.
- **CRTC** (Cathode Ray Tube Controller): the pipe that times pixels. Has a *mode* (resolution, timings) and a list of planes.
- **Plane**: an image source. Primary plane is the main framebuffer. Cursor plane is the cursor. Overlay planes for video.
- **Framebuffer**: a `drm_framebuffer` referencing a GEM BO with format/pitch.

A modern desktop has typically 1 CRTC active per display, with 1 primary plane + maybe 1 cursor plane.

## 47.2 The display path

```
GPU rendered into BO
        │
   plane.fb = BO
        │
   CRTC scans plane(s) according to mode
        │
   Encoder produces TMDS/DP
        │
   Connector outputs to physical port
        │
        Monitor
```

## 47.3 Atomic modesetting

The legacy KMS API had separate calls for each thing (`SET_CRTC`, `PAGE_FLIP`, `SET_PROPERTY`). Atomic is the modern way:

1. Build a `drm_atomic_state` containing the desired state of every modified object (CRTC, plane, connector).
2. Validate via `atomic_check`.
3. Commit via `atomic_commit`.

If anything fails validation, nothing changes. Multi-display config changes are atomic — you don't see a half-state.

ioctl: `DRM_IOCTL_MODE_ATOMIC`. `libdrm` exposes a friendly wrapper.

## 47.4 The driver's job

For each of CRTC/plane/connector/encoder, the driver implements callbacks via `drm_*_funcs` and helpers. Roughly:

- `*_atomic_check` — validate proposed state against hardware constraints.
- `*_atomic_update` (planes) — program the hardware for new image source.
- `*_atomic_enable`/`disable` (CRTC) — turn a pipe on/off.
- `*_helper_funcs` — DRM core's helpers fill in standard machinery.

amdgpu DC's task is to *map* the abstract state onto AMD's display hardware (DPP, DPG, MPC, OPP, OTG, etc.).

## 47.5 EDID and modes

When a monitor is plugged in, the connector reads the **EDID** (Extended Display Identification Data) over DDC (an I2C side channel on the display port). EDID lists supported resolutions, timings, color spaces, HDR metadata.

Driver: `connector_funcs->detect`, `connector_helper->get_modes`. `drm_get_edid()` does the I2C. `drm_add_edid_modes` parses.

## 47.6 vblank

Each CRTC emits a "vertical blank" event roughly every frame. Drivers expose:

- vblank-counter to userspace,
- vblank-event delivery via the DRM file (read `drm_event` packets).

Used for v-sync, frame pacing, and timing measurement. KMS clients call `DRM_IOCTL_WAIT_VBLANK`.

## 47.7 Page flip

The bread-and-butter operation: replace the visible framebuffer with a new one, in sync with vblank.

Atomic flip:

```c
plane.fb = new_bo;
ATOMIC_COMMIT(state, DRM_MODE_PAGE_FLIP_EVENT);
```

Driver programs the scanout-base register such that on the next vblank, the new BO becomes active. Sends a flip-complete event to userspace (over the DRM fd) so the compositor can begin building the next frame.

## 47.8 Cursor and overlay planes

- **Cursor**: small, software-positioned. AMD HW has a dedicated cursor.
- **Overlay**: secondary image surfaces, often used for video (a YUV plane behind/above primary RGB). Saves CPU/GPU time vs. compositing.

Modern compositors (Wayland) increasingly use planes to offload composition; the kernel's atomic API exposes "is this layer combination feasible?" via atomic_check.

## 47.9 HDR, color, gamma

Each plane and CRTC has properties for color management: gamma LUT, CSC matrix, degamma, color space, HDR metadata. Atomic state carries these.

amdgpu DC implements the AMD-specific color pipeline. The kernel exposes a generic API; userspace (e.g. `colord`, GameScope) drives.

## 47.10 The DC architecture (briefly)

`drivers/gpu/drm/amd/display/` contains:

- `amdgpu_dm/` — the DRM-facing layer (KMS bridge).
- `dc/` — the platform-independent DC core (also runs on Windows). Built around abstract pipe blocks.
- `dc/dcn*/` — per-generation DCN (Display Core Next) hardware code.
- `modules/` — color, freesync, hdr, mst, info_packet, etc.

DC is enormous. If you join DC, expect to spend months ramping. If you don't, treat DC as a black box callable via the standard KMS hooks.

## 47.11 Headless / KFD

GPUs without displays (server compute) or with disabled KMS (`amdgpu.modeset=0`) still load amdgpu but only expose `renderD*` and `/dev/kfd`. KMS code is bypassed. Most ROCm workloads run headless.

## 47.12 Exercises

1. `cat /sys/class/drm/card0-*/status`. Identify connected vs. disconnected.
2. Use `drmModeGetResources` (libdrm) to enumerate connectors/encoders/CRTCs/planes from userspace.
3. Read `Documentation/gpu/drm-kms.rst`.
4. Read `drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c::amdgpu_dm_atomic_check`. Don't try to absorb everything — orient.
5. Use `modetest` (from libdrm) to enumerate modes and force a particular mode (be careful, can blank your screen).

---

Next: [Chapter 48 — The amdgpu driver — top-level architecture](48-amdgpu-architecture.md).
