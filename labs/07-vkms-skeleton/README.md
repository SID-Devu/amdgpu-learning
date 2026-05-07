# Lab 07 — Read & extend `vkms`

`vkms` (Virtual Kernel Mode Setting) is the smallest complete DRM driver in tree (`drivers/gpu/drm/vkms/`). It creates a virtual display you can use even on headless machines. Reading it is the fastest way to internalize the DRM driver shape.

This lab is a **reading lab** plus a small extension: study the source, then add a feature.

## Tasks

1. **Build vkms in your kernel** (`CONFIG_DRM_VKMS=y` or `=m`). Reboot. `modprobe vkms`. `ls /sys/class/drm/` shows a new card.

2. **Read every file** in `drivers/gpu/drm/vkms/`. Identify:
   - The `struct drm_driver` declaration.
   - GEM SHMEM helpers usage.
   - CRTC, plane, encoder, connector definitions.
   - The `vblank` simulation thread (`vkms_crtc.c`).
   - Atomic check/commit functions.

3. **Run a userspace test**: `modetest` (libdrm) against the vkms card to enumerate planes/CRTCs.

4. **Add a sysfs attribute** to the vkms primary plane reporting "frames produced." Use `DEVICE_ATTR_RO`.

5. **Optional**: convert vkms's vblank emulation from `hrtimer` to `workqueue`-based. This is a small, real refactor — try sending it upstream if you complete it cleanly.

## Why no skeleton code here?

vkms itself *is* the skeleton. Writing a "vkms-clone" would be redundant; reading the real one is the lab.

Time budget: 8-12 hours.
