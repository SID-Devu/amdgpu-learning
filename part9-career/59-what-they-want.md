# Chapter 59 — What AMD / NVIDIA / Qualcomm / Intel actually look for

Time to be blunt. This chapter is the playbook for a junior with a contractor / new-grad position who wants to convert to FTE in the systems / GPU / SoC parts of these companies. The advice is gathered from real hiring loops and panel feedback; it is the same shape across all four.

## 59.1 The roles

- **Kernel / driver engineer (Linux)**: writes amdgpu / open-gpu-kernel-modules / msm / i915 code. Heavy C, kernel APIs, hardware bring-up, debugging, upstream contribution.
- **Runtime / compiler engineer**: ROCm, CUDA, Mesa, MIOpen, MIGraphX. C++ and LLVM-flavored work.
- **Tools / profiling**: rocprof, RGP, NSight. C++ and visualization.
- **GPU verification / silicon bringup**: writes test code that runs on FPGA emulators of yet-unreleased silicon.
- **HW/SW co-design**: bridges silicon and software.

This book primarily targets the first. The skills overlap heavily with the second.

## 59.2 What they're really screening for

The actual signal at AMD / NVIDIA / Qualcomm interview loops:

1. **Can you write correct, idiomatic C** and explain memory layout? (Almost every loop tests this.)
2. **Do you understand pointers, lifetimes, and undefined behavior** at the level of "I can read kernel patches and not be confused"?
3. **Concurrency**: mutexes, atomics, memory ordering, deadlocks. Always asked.
4. **Operating systems**: VM, paging, syscalls, scheduling, IRQ. Always asked.
5. **Computer architecture**: caches, coherence, branch prediction. Often asked.
6. **PCIe / DMA / MMIO**: at least one question.
7. **GPU concepts** (if you're claiming GPU exposure): wavefronts, occupancy, SIMD, fences, buffers.
8. **Debugging story**: "Tell me about the hardest bug you fixed." Real, honest answer expected.
9. **Code review / collaboration**: "How would you review this patch?" or "Walk me through your patch process."

Things that *don't* matter much for systems hires:

- LeetCode hard. (You'll see one or two graph problems, not 300.)
- Frontend / web framework knowledge.
- Cloud / kubernetes (unless the team is platform team).
- Latest ML buzzwords.

## 59.3 The bar for "competent junior" in 2026

You will be expected to:

- Build a custom kernel and out-of-tree module without help.
- Debug a kernel oops by reading a stack trace.
- Implement a producer/consumer buffer with proper synchronization.
- Write a small `ioctl`-based driver against QEMU.
- Read and explain a real DRM/amdgpu function.
- Discuss `dma-fence`, MMU, and how a Vulkan submit hits the kernel.

This book has prepared every one of those.

## 59.4 What earns you FTE conversion

Internal offers happen to people who:

- **Become the local expert** on something. "Ask Sudh about reset." Pick a sub-area; own it.
- **Land upstream patches** with their AMD email. Even tiny ones. Visible to managers.
- **Help others**: review patches, answer team questions, write internal docs.
- **Communicate clearly**. Standups, design docs, emails. Strong written communication is often the deciding factor for converting.
- **Don't argue with hardware.** When the silicon does something weird, you accept and adapt; you don't blame the spec.
- **Understand customer use cases.** A patch that solves a real game/app issue reads better than a "cleanup."

A blunt truth: the contractor-to-FTE path at AMD/NVIDIA/Qualcomm is a **performance review** dressed up as recruitment. By the time the requisition opens, the team usually knows whom they want. Be that person before the req opens.

## 59.5 The 90-day plan when you join

If you've just joined as a contractor in the amdgpu team:

**Days 1-30**: build, boot, and load a custom amdgpu on hardware you have. Read every IP block's `_init` function. Triage one open bug in your area to the line.

**Days 31-60**: ship one small upstream patch (typo, dmesg improvement, missing const). Get it merged. Onboard to internal tooling, the test farm, the bug tracker.

**Days 61-90**: own one component (say, `gfx_v11_0.c`) at the level of "I can review patches there." Write or update one piece of internal documentation. Pick a real customer-facing bug and own it through fix.

By day 100, your manager should be saying "this contractor is doing FTE-grade work." That gets the requisition opened.

## 59.6 Companies' cultures (rough)

- **AMD**: open-source-heavy, mailing-list culture, distributed teams, Indian + Eastern-European + N. American hubs. Output-focused; hardware-spec discipline.
- **NVIDIA**: closed-source kernel module historically; OSS now (open-gpu-kernel-modules). High intensity, deep specialization. Expect HW NDA early.
- **Qualcomm**: SoC-centric (Adreno GPU on Snapdragon). msm DRM, more downstream Android-focused. Carrier requirements drive a lot of work.
- **Intel**: i915/Xe DRM open. Strong in tooling, perf engineering. Different products different teams.

Pick your battles where the culture fits.

## 59.7 The non-technical signals

- **Punctuality** matters. Don't be late to scheduled syncs.
- **Replying** to email/Slack quickly with thoughtful answers.
- **Asking** good questions: scoped, with context, with what you've tried.
- **Saying "I don't know"** when you don't, then finding out.
- **Avoiding** bikeshedding (style nits in code review when they don't matter).

These are real factors in conversion decisions.

## 59.8 Exercises

1. List three areas of amdgpu you could plausibly own in 6 months. Pick one.
2. Subscribe to amd-gfx@lists.freedesktop.org. Spend a week just reading patches. Notice the rhythm.
3. Pick one open issue from `gitlab.freedesktop.org/drm/amd/-/issues` and read every comment. Form an opinion.
4. Identify two engineers on your team whose work you admire. Read their last 20 commits. Note what they value.
5. Write a one-page personal "what I will own this quarter" doc. Share with your manager.

---

Next: [Chapter 60 — System-software interview question bank with answers](60-interview-questions.md).
