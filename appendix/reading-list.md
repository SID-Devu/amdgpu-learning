# Appendix — Curated reading list

Books, talks, and source you will actually open. Curated, not a dump.

## Books — read end-to-end

1. **The C Programming Language**, Kernighan & Ritchie. Still the cleanest C book. ~270 pages.
2. **Effective Modern C++**, Scott Meyers. 42 items, every one earns its keep. C++11/14, but the lessons map to C++17/20.
3. **Operating Systems: Three Easy Pieces** (free online). Best modern OS textbook for a beginner.
4. **Linux Device Drivers, 3rd ed.** (Corbet/Rubini/Kroah-Hartman, free PDF). Old (2.6 era) but still the best driver book ever written. Read it knowing some APIs have evolved.
5. **Linux Kernel Development, 3rd ed.**, Robert Love. Friendly tour of the kernel.
6. **Understanding the Linux Kernel**, Bovet & Cesati. Heavier reference; dip in.
7. **Computer Architecture: A Quantitative Approach**, Hennessy & Patterson. Skim early chapters; deep on memory hierarchy & ILP.
8. **The Linux Programming Interface**, Michael Kerrisk. The definitive Linux syscall book.

## Books — reference (not cover-to-cover)

* **C++ Concurrency in Action**, Anthony Williams.
* **Is Parallel Programming Hard?**, Paul McKenney (free). RCU and memory models from the inventor.
* **Memory Barriers: a hardware view for software hackers**, McKenney (paper).
* **What Every Programmer Should Know About Memory**, Ulrich Drepper (free PDF). Old but the cache chapter is timeless.

## Documentation you should bookmark

* `Documentation/` in the kernel tree — especially `Documentation/gpu/`, `Documentation/locking/`, `Documentation/driver-api/`, `Documentation/process/`.
* The DRM kernel-doc HTML: build it once with `make htmldocs` and read `gpu.html`.
* AMD GPUOpen + ROCm docs: <https://gpuopen.com>, <https://rocmdocs.amd.com>.
* PCIe base spec (you likely won't read it; you will *grep* it).

## Talks (YouTube — search and watch)

* "The C++ Programming Language" — Bjarne Stroustrup, Going Native 2012.
* "atomic<> Weapons" parts 1 & 2 — Herb Sutter. Memory model, slow but worth it.
* "Linux Graphics, GPUs and DRM" — Daniel Vetter, FOSDEM. Multiple years; pick the latest.
* "How the Linux Kernel ARM page tables work" — Linus Walleij.
* "An introduction to the LLVM compiler infrastructure" — Chris Lattner.
* AMD ISA + GPU architecture deep dives (RDNA whitepapers, CDNA whitepapers).
* "Diagnosing realtime application issues" — Steven Rostedt, ftrace creator.

## Source code (read this, not just docs)

* `drivers/gpu/drm/vkms/` — smallest complete DRM driver.
* `drivers/gpu/drm/drm_*.c` core files — `drm_drv.c`, `drm_ioctl.c`, `drm_gem.c`, `drm_atomic.c`, `drm_atomic_helper.c`.
* `drivers/gpu/drm/scheduler/sched_main.c` — the DRM scheduler.
* `drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c`, `amdgpu_device.c`, `amdgpu_cs.c`, `amdgpu_vm.c`, `amdgpu_ring.c`, `amdgpu_fence.c`, `amdgpu_irq.c`, `amdgpu_gem.c`, `amdgpu_bo.c`.
* `drivers/gpu/drm/ttm/ttm_*.c` — buffer manager.
* `mesa/src/amd/` and `mesa/src/gallium/drivers/radeonsi/`, `mesa/src/amd/vulkan/` (radv) — the userspace side.
* libdrm `amdgpu/` directory.
* For RCU: `kernel/rcu/tree.c` — but read McKenney's papers first.

## Mailing lists / forums

* `linux-kernel@vger.kernel.org` (lore.kernel.org) — main kernel list.
* `dri-devel@lists.freedesktop.org` — DRM/KMS/GPU devs (this is where amdgpu patches go).
* `amd-gfx@lists.freedesktop.org` — AMD-specific.
* `mesa-dev` / Mesa GitLab (most discussion is on MRs now).
* `freedesktop.org` IRC: #dri-devel, #radeon (libera.chat).

## Reading order for an absolute beginner

Months 1–2: K&R + arena/SPSC/thread-pool labs.
Months 3–4: OSTEP + LDD3 chapters 1-7 + your own char driver + edu PCI lab.
Months 5–6: Drepper memory paper + McKenney's "Is Parallel Programming Hard?" + DRM core source.
Months 7–9: amdgpu source crawl + Mesa radv + first upstream patch.
Months 10–12: Specialize (RAS, KFD, scheduler, DC, ...). Your specialty becomes your interview answer.
