# Chapter 87 — Cross-stack debug & regression triage

A bug at PMTS level rarely has a single cause in a single layer. It spans:

```
App ↔ runtime ↔ UMD ↔ kernel ↔ firmware ↔ hardware ↔ device
```

You must move fluidly between all of them. This chapter is the playbook.

## The triage funnel

```
Customer report
   │
   v
Reproduce
   │
   v
Localize  ←─ which layer?  app / UMD / kernel / FW / HW
   │
   v
Bisect (in time, in config, in workload)
   │
   v
Root cause
   │
   v
Fix + test + push upstream + backport
   │
   v
Close
```

Each arrow is a *skill*. Most engineers are great at 2 of these and adequate at the rest. PMTS engineers are great at all of them.

## Reproducing

Without a repro, you have a story, not a bug. First step is always "make it happen on demand."

Tactics:

- Get exact game build, driver version, kernel, firmware, GPU, OS.
- Get a save file at the moment before crash.
- Capture the API call stream (`vkapidump`, `apitrace`, `vktrace`).
- Synthetic reproducers from API traces.
- Loops and stress tests.

If you can't reproduce in a controlled environment, you can't trust any fix. **Spend the time.**

## Localizing the layer

Each layer has signatures:

| Symptom | Likely layer |
|---|---|
| `dmesg: amdgpu: VM_FAULT...` | kernel saw GPU fault — UMD or app bug usually |
| `VK_ERROR_DEVICE_LOST` | reset happened — could be anywhere |
| `glxgears` works, game doesn't | game-specific UMD or shader compile bug |
| Always after game's "compiling shaders" | PSO / shader compiler issue |
| Random hang after hours | TDR or memory leak |
| Tearing, jitter on display | KMS / DC / VRR |
| Slow only on one GPU model | firmware version / SMU |
| Runs fine elsewhere except on AMD | Mesa / radv / kernel bug |
| Same crash on Windows D3D12 | likely hardware / firmware |

Differential diagnosis: change one variable and see what changes.

- Different GPU? Kernel? Firmware? Mesa version?
- Disable one feature: `RADV_DEBUG=nodcc nooptimize`.
- Boot with `amdgpu.gpu_recovery=0` to inspect post-hang state.

## Bisecting

If a regression appeared between two known-good and known-bad versions, **`git bisect`** is your friend.

For Mesa / kernel:

```bash
cd ~/work/linux
git bisect start
git bisect bad master
git bisect good v6.6
# bisect builds, you boot/test, mark good or bad
git bisect good   # or: git bisect bad
# after ~log2(N) iterations, you have one commit
```

Build/boot cycles are slow. Tips:

- Pre-build a minimal kernel config that's fast.
- Use `make -j$(nproc)`.
- VM with virtfs sharing source/build avoids reinstalls.
- Script the bisect step: `git bisect run my-test.sh` if the test is automatable.

For firmware bisects: AMD ships discrete firmware versions; you bisect by manually swapping `*_smc.bin` etc. and rebooting.

## Reading dmesg

Patterns to recognize at a glance:

```
[  120.123] amdgpu 0000:03:00.0: GPU fault detected: 147 0x09380000 ...
            ^                                       ^
            adev (PCI BDF)                          fault status code

[  120.124] amdgpu: VM_FAULT_FROM_ME at addr 0x0000000040004000 ...
                                       ^
                                       faulting GPU VA

[  120.125] amdgpu: GFX 0 wave 0 lane mask 0xffffffff ...
                            ^
                            offending wave
```

Decode the status code from `gmc_v*.h`. Sub-codes tell you whether it's a translation fault, a permission fault, a write-to-RO, etc. — narrows the hypothesis.

## Reading trace-cmd output

```bash
trace-cmd record -e 'amdgpu_cs_ioctl' -e 'dma_fence_*' -e 'drm_sched_*' my_game
trace-cmd report > trace.txt
```

You get a microsecond-accurate timeline of submissions, fences, and scheduler events. A typical session reveals patterns like:

- Fence not signaling → `drm_sched_*` shows job stuck.
- Long latency between submit and start → CPU-bound or scheduler blocked.
- Many short submissions → CPU overhead opportunity.

## umr — direct hardware introspection

```bash
sudo umr -lb gfx_0.0.0      # live wave + ring state for GFX
sudo umr -O ihn -s gfx_0.0.0 # IH ring contents
sudo umr -p REG_NAME         # program a register
```

`umr` reads/writes registers, decodes ring buffers, dumps wave state. Used when nothing else works.

## rocgdb walkthrough

For a hung HIP kernel:

```bash
rocgdb my_app
(rocgdb) run
^C    # interrupt while kernel is running
(rocgdb) info threads
(rocgdb) thread <gpu thread>
(rocgdb) bt
(rocgdb) info locals
(rocgdb) p $vgpr0
```

You can step single instructions, inspect VGPRs, see exec mask. When it works, it's magic. Setup is painful (debug builds, matching ROCm/kernel/firmware versions).

## Cross-stack reasoning example

**Scenario**: A new RDNA3 chip exhibits visual corruption only in Cyberpunk 2077, only with DXR enabled, only at 4K, only on Linux. (Hypothetical.)

Steps:

1. **Reproduce**: confirmed on a couple of machines.
2. **Layer hypothesis**: graphics-only? Compute? Display?
   - Disable DXR → bug gone → narrows to ray tracing path.
3. **Layer further**:
   - Disable `RADV_DEBUG=nooptimize` → bug remains. So not an ACO regression.
   - Try AMDVLK (closed-source AMD Vulkan) → bug remains. So not a radv-only bug.
   - Try Windows → bug not there. So Linux-specific.
4. **Hypothesis**: kernel or firmware path differs Linux vs Windows for RT features.
5. **Bisect kernel**: between two kernels, find the patch that introduced — turns out to be a TLB-flush optimization.
6. **Root cause**: optimization missed flushing for a specific BVH-update flow; stale TLB entries cause wrong reads.
7. **Fix**: revert or refine the optimization.
8. **Test**: reproduce no longer triggers across multiple machines + games.
9. **Push upstream**: with `Fixes:` tag, `Cc: stable@vger.kernel.org`.

A real example like this can take days to weeks. Senior engineers maintain a mental library of "where similar problems came from" so they can short-circuit the funnel.

## When the bug is in firmware

If your bisect lands on "no kernel/UMD change, same firmware" — you may have a hardware/firmware errata. Steps:

1. Search AMD's internal bug DB for similar reports.
2. Check firmware release notes for related fixes.
3. Submit a bug to the appropriate AMD team with the repro.
4. While waiting: see if a kernel-side workaround is feasible (e.g., disable an HW feature on this chip, write extra TLB flushes).

## When it's an upstream regression you didn't cause

Etiquette:

- File a bug on the appropriate tracker (Mesa GitLab, kernel bugzilla, lore.kernel.org).
- Tag the original commit author and any reviewers.
- Provide repro and bisect.
- Be polite. Software is hard.

## Production patterns

A real PMTS workflow looks like:

```
Morning: triage 3-5 new bugs
   - reproduce 2, file the others for more info
Mid-morning: deep dive on biggest customer hit
   - bisect, root cause, prepare fix
Afternoon: review 3 patches from team
Evening: write tests, send fix patches upstream
```

Mixed with: meetings, planning, mentoring junior engineers, talking to customers.

## What you should be able to do after this chapter

- Reproduce, localize, bisect, root-cause, fix, push upstream — independently.
- Use dmesg, trace-cmd, umr, rocgdb without panic.
- Follow a hypothesis tree across layers.
- Recognize firmware vs kernel vs UMD bug patterns.

## Read deeper

- "Linux Kernel Debugging" book (recent editions).
- AMD GPUOpen "Crash debugging on Radeon."
- `Documentation/admin-guide/bug-bisect.rst`.
- DEFCON / FOSDEM talks on real driver debug stories.
- The amd-gfx@lists.freedesktop.org archive — months of cross-stack debug discussions.
