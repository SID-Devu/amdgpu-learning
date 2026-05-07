# Chapter 53 — Reset, recovery, RAS, and the real world

GPUs hang. Memory has bit flips. Firmware faults. Production systems must recover gracefully. Reset and RAS code is amdgpu's **highest-leverage** area for a junior — small, important patches in this region get attention and ownership.

## 53.1 What "reset" means

The driver detects something has gone wrong (job timed out, register-poll hung, memory error, page fault) and:

1. Stop new submissions.
2. Quiesce in-flight (mark fences with errors).
3. Reset hardware (one or more IPs, possibly the whole GPU).
4. Reload firmware.
5. Re-init state (rings, VMIDs, page tables, display).
6. Resume scheduling.

Done well, a single bad shader doesn't crash the desktop; only the offending GPU job's process sees `VK_ERROR_DEVICE_LOST`.

## 53.2 Reset levels (modes)

amdgpu defines several:

- **soft reset** — reset specific IPs (GFX, SDMA) only.
- **mode-1 reset** — full ASIC reset, but PCIe link stays.
- **mode-2 reset** — partial ASIC reset.
- **PCI FLR (Function-Level Reset)** — last resort; PCIe-level.
- **whole-GPU reset** — combines everything; full re-init.

Selection depends on ASIC, error severity, and platform support. Module param `amdgpu.gpu_recovery` controls.

## 53.3 The recovery code path

Entry: `amdgpu_device_gpu_recover()`. Long function. Outline:

```c
amdgpu_device_lock_adev();              /* exclusive */
drm_sched_stop(all schedulers);
   ↓
quiesce all rings
amdgpu_device_pre_asic_reset();         /* kill jobs */
   ↓
amdgpu_asic_reset();                    /* the actual reset */
amdgpu_device_post_asic_reset();
   ↓
amdgpu_device_ip_resume();              /* re-init IPs */
   ↓
drm_sched_start(all schedulers);
amdgpu_device_unlock_adev();
```

Sub-paths handle:

- per-process VMs (re-bind PTEs, evicted BOs reload).
- KFD queues (re-create, doorbell ring).
- DC display (re-program OTG, re-light displays).
- PSP firmware reload.

The function is large because every IP must contribute correct shutdown/restart code. Buggy IP code = recovery hangs.

## 53.4 What the userspace sees

A submitting process whose job was on a reset gets:

- The fence signaled with `-ECANCELED` (or `-ENODEV` for PCI death).
- Subsequent ioctls return errors until the process re-creates its resources.

Mesa/`radv` translates this to `VK_ERROR_DEVICE_LOST`. Vulkan apps must implement device-lost handling — most don't and just crash. Compositors usually re-init and recover.

## 53.5 RAS — Reliability, Availability, Serviceability

For data-center GPUs (Instinct MI series), bit flips matter. ECC memory, parity in caches, register-level error reporting. The driver:

- Gathers correctable / uncorrectable errors.
- Exposes via sysfs (`/sys/class/drm/card*/device/ras/`).
- Triggers a full reset on fatal errors.
- Reports counts to userspace (rocm-smi).

`drivers/gpu/drm/amd/amdgpu/amdgpu_ras.c` is the core. Per-IP `*_ras.c` files do the IP-specific error decoding.

## 53.6 Hang detection

Kicked in at multiple layers:

- `drm_sched_job` timeout — job exceeds `drm_gpu_scheduler.timeout`.
- Kernel watchdog on internal poll loops (`amdgpu_ring_test_helper`).
- Userspace MTBF heuristics (Mesa watchdog, KFD).
- Hardware-driven RAS errors via IH ring.

A "hang" can be: shader infinite loop, bad memory access, cache deadlock, link error.

## 53.7 Page-fault path

When a GPU dereferences an unmapped GPUVA:

1. The hardware raises a fault, posts to IH ring.
2. amdgpu's `gmc_v*::process_interrupt` decodes (which client, address, RW).
3. If recoverable (SVM): kernel resolves; tells GPU to retry.
4. If not: log + kill the offending job (or whole context); reset if necessary.

Most recoverable faults today come from SVM/KFD. Graphics typically uses pre-bound memory.

## 53.8 The watchdog and the "TDR" experience

Windows-style **TDR** (Timeout Detection and Recovery) — a job stuck for >2 s triggers reset; user sees a brief screen flicker. Linux's amdgpu has equivalent via `drm_sched`. The default 10 s timeout is conservative; for compute, you may want longer.

Module param `amdgpu_lockup_timeout=` lets users configure per ring.

## 53.9 Production-quality testing

For driver work, your patches will be expected to handle:

- Stress tests (`piglit`, `vkcube --persistent`, `glmark2` long runs).
- Reset injection — `echo 1 > /sys/kernel/debug/dri/0/amdgpu_gpu_recover` triggers a fake reset; observe that everything recovers.
- Fault injection — kfd-ras has interfaces to inject errors.
- Memory pressure — fill VRAM, force eviction, observe correctness.
- Suspend/resume loops — `rtcwake -m mem -s 5` in a loop.

Doing this on your own machine *before* submitting upstream patches is the difference between "merged in v2" and "rejected with months of back-and-forth."

## 53.10 What junior engineers can land

If you want quick, real, valuable patches:

- Improve dmesg messages in error paths (more useful info, less noise).
- Add tracepoints for newly-tracked events.
- Fix small `WARN_ON`s that fire under specific configurations.
- Improve reset coverage for less-tested combinations (e.g. multi-GPU setups).
- Add new sysfs/debugfs files exposing internal state for triage.
- Document something that's not documented (`Documentation/gpu/amdgpu/...`).

Each is low-risk, high-value, and teaches you the surrounding code.

## 53.11 Example real-world patch shape

```
Subject: drm/amdgpu: avoid a NULL deref in ras_recovery on init failure

When the PSP fails to initialize during early probe, ras_recovery
attempts to dereference psp->ring before it is allocated, causing a
kernel oops.

Fix it by checking for ring presence before recovery.

Signed-off-by: <you>

---
 drivers/gpu/drm/amd/amdgpu/amdgpu_ras.c | 3 +++
 1 file changed, 3 insertions(+)
```

A two-line fix gets you on the contributor list. From there, slowly take on bigger pieces.

## 53.12 Exercises

1. Read `amdgpu_device.c::amdgpu_device_gpu_recover` in full. Sketch a flow diagram on paper.
2. Trigger a manual reset: `echo 1 > /sys/kernel/debug/dri/0/amdgpu_gpu_recover`. Observe dmesg. Confirm the desktop recovers.
3. Read `amdgpu_ras.c::amdgpu_ras_block_late_init`.
4. Search the amd-gfx mailing list for "regression" patches; observe how they're discussed and merged.
5. Pick one open bug from the AMD bug tracker and attempt to reproduce. (You'll be surprised how often the answer is "didn't reproduce, please test.")

---

End of Part VII. Move to [Part VIII — Hardware/software interface](../part8-hw-sw/54-comp-arch.md).
