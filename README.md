# From Zero to AMD GPU Kernel Engineer

> A self-contained, deeply technical textbook covering C, C++, operating systems, multithreading, the Linux kernel, Linux device drivers, and the AMD GPU (DRM/amdgpu) stack — written for a fresh graduate aiming at an FTE role at **AMD, NVIDIA, Qualcomm, Intel, Broadcom, MediaTek, Marvell** or any low-level systems / GPU / SoC team.

This book assumes **no prior programming knowledge at all**. We start from "what is a computer" and "how do I open a terminal" in **Part 0**. By the end (Part IX) you can:

- read and modify `drivers/gpu/drm/amd/amdgpu/*.c`,
- write a PCIe device driver from scratch,
- explain command submission, fences, TTM, and KMS in an interview,
- pass system-software interview loops at AMD/NVIDIA/Qualcomm.

---

## ⚠️ NEW — Read the LEARNING PATH first

**Before anything else, read [LEARNING-PATH.md](LEARNING-PATH.md).** It tells you what order to read this book in (you do **not** read parts numerically the first time — there's a real path).

The 14-stage path: computer literacy → programming basics → C → **DSA** → **userspace systems programming** → C++ → multithreading → **OS theory** → kernel → drivers → GPU → HW/SW → career → senior track.

If reading "Chapter 1: What is a program, a compiler, an OS?" feels too advanced, **you are in the right place but on the wrong page**. Go to **[Part 0 — Absolute Zero](part0-zero/README.md)** first. It assumes you know nothing — not even how to use a terminal — and brings you up to the level where Part I makes sense.

If you already know basic Linux + Python or a little C, skim Part 0 and start at Part I.

---

## How to use this book

1. **Read sequentially the first time.** Every part builds on the previous.
2. **Type every code example.** Don't copy-paste. Muscle memory matters.
3. **Do the labs** in `labs/` after each part.
4. **Re-read** the GPU parts after you've shipped your first kernel module.

Estimated time: **6–9 months** at 2 hrs/day, or **3 months** full-time. Add 2–3 weeks if you're starting from Part 0.

You will build, in order:

- a C memory allocator,
- a C++ smart pointer & a tiny STL,
- a userspace cooperative scheduler,
- a producer/consumer ring buffer (lock-based and lock-free),
- a Linux kernel module,
- a character device driver with `ioctl` and `mmap`,
- a virtual PCI driver against QEMU's `edu` device,
- a tiny DRM driver,
- a userspace GPU command submitter using `libdrm` against amdgpu.

---

## Table of Contents

### Part 0 — Absolute Zero (no programming knowledge required)
0. [Start here](part0-zero/README.md)
1. [What is a computer? (in plain English)](part0-zero/01-what-is-a-computer.md)
2. [Using Linux: the terminal](part0-zero/02-using-linux.md)
3. [Files and paths](part0-zero/03-files-and-paths.md)
4. [Text editors](part0-zero/04-text-editors.md)
5. [What is code?](part0-zero/05-what-is-code.md)
6. [Your first program (in Python)](part0-zero/06-first-program-python.md)
7. [Decisions and repeats](part0-zero/07-decisions-and-repeats.md)
8. [Functions](part0-zero/08-functions.md)
9. [From Python to C](part0-zero/09-from-python-to-c.md)
10. [When things break — debugging mindset](part0-zero/10-when-things-break.md)
11. [Bridge to Part I](part0-zero/11-bridge-to-part1.md)

### Part DSA — Data Structures & Algorithms (in C) — *Stage 4 in the [LEARNING PATH](LEARNING-PATH.md)*

> **READ AFTER PART I.** Required for interviews **and** kernel work. 28 chapters.

See **[partDSA/README.md](partDSA/README.md)** for the full table of contents. Topics: complexity, arrays, strings, linked lists, stacks/queues, hash tables, BSTs, RB/AVL trees, B-trees, heaps, graphs, shortest paths, tries, all the sorts, all the searches, recursion, DP, greedy, divide & conquer, bit tricks, two pointers, backtracking, union-find, segment / Fenwick / interval trees, KMP / Rabin-Karp / suffix arrays, system-design DS (LRU, bloom filter, skip list, consistent hashing), concurrent DS (RCU, lock-free queues), and a guided tour of the kernel's data structures.

### Part USP — Userspace Systems Programming — *Stage 5 in the [LEARNING PATH](LEARNING-PATH.md)*

> **MASTER USERSPACE FIRST. THEN KERNEL.** 12 chapters.

See **[partUSP/README.md](partUSP/README.md)** for the full table of contents. Topics: Unix philosophy & syscalls, processes (fork/exec/wait), signals, pipes/FIFOs, shared memory & mmap, message queues / semaphores / futex, sockets (TCP/UDP/Unix), epoll / io_uring, pthreads deep, file I/O deep (`O_DIRECT`, `sendfile`), `/proc` / `/sys` / `/dev`, perf tools (strace, perf, valgrind, gdb).

### Part OST — Classic OS Theory — *Stage 8 in the [LEARNING PATH](LEARNING-PATH.md)*

> Academic foundation that complements Part III (Linux-specific). 10 chapters.

See **[partOST/README.md](partOST/README.md)** for the full table of contents. Topics: process model, scheduling theory (FCFS/SJF/RR/MLFQ/lottery/CFS/EDF), synchronization theory (Peterson/semaphores/monitors), classic problems (dining philosophers etc.), deadlock, memory theory (paging/segmentation/swap), page replacement, file system theory & journaling, crash consistency, distributed systems intro (consensus, RPC, time, CAP).

### Part I — C from Zero to Driver-Grade
1. [What is a program, a compiler, an OS?](part1-c/01-what-is-a-program.md)
2. [Your first C programs and the toolchain](part1-c/02-toolchain-and-first-programs.md)
3. [Types, integer promotion, undefined behavior](part1-c/03-types-and-ub.md)
4. [Pointers, arrays, the memory model of C](part1-c/04-pointers-and-memory.md)
5. [Structs, unions, bitfields, packing, alignment](part1-c/05-structs-unions-alignment.md)
6. [Bit manipulation — the language of hardware](part1-c/06-bit-manipulation.md)
7. [Dynamic memory and writing your own allocator](part1-c/07-allocators.md)
8. [The preprocessor, headers, linkage, build systems](part1-c/08-preprocessor-and-build.md)
9. [Function pointers, callbacks, opaque types, plugin design](part1-c/09-function-pointers.md)
10. [Defensive C: const correctness, restrict, MISRA-ish style](part1-c/10-defensive-c.md)

### Part II — C++ for Systems Programmers
11. [From C to C++: classes, RAII, references](part2-cpp/11-c-to-cpp.md)
12. [Object lifetime, special members, rule of 0/3/5](part2-cpp/12-lifetime-and-rules.md)
13. [Templates, concepts, and the type system](part2-cpp/13-templates.md)
14. [Modern C++: move semantics, smart pointers, lambdas](part2-cpp/14-modern-cpp.md)
15. [STL and writing your own container](part2-cpp/15-stl-and-containers.md)
16. [C++ for low-level work: PIMPL, polymorphism cost, ABI](part2-cpp/16-cpp-low-level.md)

### Part III — Operating Systems Internals
17. [What an OS is and the boot path on x86_64](part3-os/17-what-is-an-os.md)
18. [Processes, threads, address spaces, syscalls](part3-os/18-processes-threads-syscalls.md)
19. [Virtual memory, paging, TLBs, MMUs](part3-os/19-virtual-memory.md)
20. [Scheduling — CFS, deadline, real-time](part3-os/20-scheduling.md)
21. [The I/O stack — VFS, page cache, block layer](part3-os/21-io-stack.md)
22. [Interrupts, exceptions, IRQ delivery](part3-os/22-interrupts.md)

### Part IV — Multithreading & Concurrency
23. [Threads, the C/C++ memory model](part4-multithreading/23-threads-and-memory-model.md)
24. [Mutexes, condition variables, semaphores](part4-multithreading/24-locks-and-condvars.md)
25. [Atomics, lock-free data structures](part4-multithreading/25-atomics-and-lockfree.md)
26. [The Linux kernel concurrency toolbox](part4-multithreading/26-kernel-concurrency.md)
27. [GPU concurrency — fences, queues, doorbells](part4-multithreading/27-gpu-concurrency.md)

### Part V — Linux Kernel Foundations
28. [The kernel source tree and how to build it](part5-kernel/28-kernel-tree-and-build.md)
29. [Loadable kernel modules — your first module](part5-kernel/29-modules.md)
30. [Kernel data structures: lists, rbtrees, xarray, idr](part5-kernel/30-kernel-data-structures.md)
31. [Kernel memory: kmalloc, vmalloc, slab, page allocator](part5-kernel/31-kernel-memory.md)
32. [Time, timers, workqueues, kthreads](part5-kernel/32-time-and-deferred-work.md)
33. [Locking in the kernel](part5-kernel/33-kernel-locking.md)
34. [The driver model — kobject, sysfs, devices, drivers, buses](part5-kernel/34-driver-model.md)

### Part VI — Linux Device Drivers
35. [Character drivers from scratch](part6-drivers/35-char-drivers.md)
36. [`ioctl`, `mmap`, polling, async I/O](part6-drivers/36-ioctl-mmap-poll.md)
37. [The PCI/PCIe subsystem and writing a PCI driver](part6-drivers/37-pci.md)
38. [DMA, IOMMU, coherent vs streaming mappings](part6-drivers/38-dma-and-iommu.md)
39. [MMIO, register access, barriers](part6-drivers/39-mmio-and-barriers.md)
40. [Power management, runtime PM, suspend/resume](part6-drivers/40-power-management.md)
41. [debugfs, sysfs, tracing, ftrace, perf](part6-drivers/41-debug-and-trace.md)

### Part VII — The Linux GPU Stack (DRM + amdgpu)
42. [GPU architecture for software engineers](part7-gpu/42-gpu-architecture.md)
43. [The Linux graphics stack from app to silicon](part7-gpu/43-graphics-stack.md)
44. [DRM — the kernel's GPU framework](part7-gpu/44-drm-core.md)
45. [GEM — buffer objects](part7-gpu/45-gem.md)
46. [TTM — the memory manager amdgpu uses](part7-gpu/46-ttm.md)
47. [KMS — display, planes, CRTCs, atomic modesetting](part7-gpu/47-kms.md)
48. [The amdgpu driver — top-level architecture](part7-gpu/48-amdgpu-architecture.md)
49. [Command submission, IBs, rings, doorbells](part7-gpu/49-command-submission.md)
50. [The GPU scheduler and dma-fence](part7-gpu/50-scheduler-and-fences.md)
51. [VM, page tables, and unified memory](part7-gpu/51-gpu-vm.md)
52. [Userspace: libdrm, Mesa, ROCm, KFD](part7-gpu/52-userspace.md)
53. [Reset, recovery, RAS, and the real world](part7-gpu/53-reset-and-recovery.md)

### Part VIII — Hardware/Software Interface
54. [Computer architecture for systems programmers](part8-hw-sw/54-comp-arch.md)
55. [Caches, coherence, and memory ordering](part8-hw-sw/55-caches-and-ordering.md)
56. [PCIe deep dive](part8-hw-sw/56-pcie.md)
57. [Performance: profiling, flamegraphs, GPU profilers](part8-hw-sw/57-performance.md)
58. [Debugging: gdb, kgdb, crash, ftrace, dmesg, lockdep](part8-hw-sw/58-debugging.md)

### Part IX — Getting Hired (AMD / NVIDIA / Qualcomm / Intel)
59. [What these companies actually look for](part9-career/59-what-they-want.md)
60. [System-software interview question bank with answers](part9-career/60-interview-questions.md)
61. [Projects that get you hired](part9-career/61-portfolio-projects.md)
62. [Contributing to the upstream kernel and Mesa](part9-career/62-upstream-contribution.md)
63. [The 12-month plan from contractor to FTE](part9-career/63-twelve-month-plan.md)

### Part X — The Senior / Principal / PMTS Track (the 24-year-engineer's map)
*See [`partX-principal/README.md`](partX-principal/README.md) for how to use this part. Reference material — not a tutorial.*

**Hardware foundations**
64. [GPU microarchitecture: RDNA](partX-principal/64-microarch-rdna.md)
65. [GPU microarchitecture: CDNA & matrix engines](partX-principal/65-microarch-cdna.md)
66. [Wavefronts, occupancy, VGPRs/SGPRs](partX-principal/66-waves-occupancy-gprs.md)
67. [GPU memory subsystem: HBM, GDDR, controllers](partX-principal/67-memory-subsystem.md)
68. [GPU cache hierarchy & coherence](partX-principal/68-gpu-cache-hierarchy.md)
69. [PM4 packets, command processor, firmware engines](partX-principal/69-pm4-firmware.md)

**The graphics pipeline, deeply**
70. [Geometry pipeline: vertex → tess → geom → mesh shaders](partX-principal/70-geometry-pipeline.md)
71. [Rasterization, hierarchical Z, primitive culling](partX-principal/71-rasterization.md)
72. [Pixel pipeline: RBE, ROPs, blending, MSAA](partX-principal/72-pixel-rbe.md)
73. [Ray tracing hardware](partX-principal/73-ray-tracing.md)

**Compute, runtime, queues**
74. [KFD, HSA, AQL packets, MES](partX-principal/74-kfd-hsa-aql.md)
75. [CWSR, priorities, preemption](partX-principal/75-cwsr-preempt.md)
76. [ROCm runtime deep](partX-principal/76-rocm-runtime.md)

**Display, multimedia, power**
77. [Display Core (DCN) deep dive](partX-principal/77-display-core-dcn.md)
78. [Multimedia (VCN), codecs](partX-principal/78-multimedia-vcn.md)
79. [SMU, DPM, voltage/frequency, telemetry](partX-principal/79-smu-power.md)

**Userspace stack deep**
80. [Mesa, radv, ACO, NIR](partX-principal/80-mesa-radv-aco.md)
81. [LLVM AMDGPU backend](partX-principal/81-llvm-amdgpu-backend.md)
82. [Vulkan & DX12 deep](partX-principal/82-vulkan-dx12.md)

**Driver advanced topics**
83. [SR-IOV and GPU virtualization](partX-principal/83-sriov-virt.md)
84. [VM faults, page tables, recovery](partX-principal/84-vm-fault-recovery.md)
85. [Security: PSP, FW signing, attestation, SEV](partX-principal/85-security-psp.md)

**Performance & debug at scale**
86. [SQTT, RGP, perf counters, RGD](partX-principal/86-sqtt-rgp.md)
87. [Cross-stack debug & regression triage](partX-principal/87-cross-stack-debug.md)

**Engineering at staff/principal level**
88. [Customer / ISV / game studio interaction](partX-principal/88-customer-isv.md)
89. [Maintainership and upstream leadership](partX-principal/89-maintainership.md)
90. [Silicon bring-up, post-silicon validation](partX-principal/90-silicon-bringup.md)
91. [Cross-vendor architecture: NVIDIA, Apple, ARM, Imagination](partX-principal/91-cross-vendor.md)
92. [The PMTS career arc — the 24-year view](partX-principal/92-pmts-career.md)

### Labs
See [`labs/README.md`](labs/README.md) — runnable C, C++, kernel, and DRM exercises:

1. [`01-arena/`](labs/01-arena/) — arena allocator (C)
2. [`02-spsc/`](labs/02-spsc/) — lock-free SPSC ring buffer (C++)
3. [`03-thread-pool/`](labs/03-thread-pool/) — mutex+condvar thread pool (C++)
4. [`04-hello-mod/`](labs/04-hello-mod/) — first kernel module
5. [`05-myring-chardev/`](labs/05-myring-chardev/) — char driver with `read`/`write`/`poll`/`ioctl`
6. [`06-edu-pci/`](labs/06-edu-pci/) — PCI driver against QEMU's `edu` device
7. [`07-vkms-skeleton/`](labs/07-vkms-skeleton/) — read & extend the in-tree minimal DRM driver
8. [`08-libdrm-amdgpu/`](labs/08-libdrm-amdgpu/) — userspace amdgpu BO + IB submission

### Appendix
- [Toolchain cheat sheet](appendix/toolchain.md) — build, debug, kernel commands
- [Kernel coding style](appendix/kernel-style.md) — the rules that get patches accepted
- [Reading the amdgpu source map](appendix/amdgpu-source-map.md) — guided tour of `drivers/gpu/drm/amd/amdgpu/`
- [QEMU + custom-kernel dev VM](appendix/qemu-and-kernel-dev-vm.md) — your daily reproducer
- [Reading list](appendix/reading-list.md) — books, papers, talks, source
- [Checklists](appendix/checklists.md) — oops, hang, patch, review, day-zero
- [Glossary](appendix/glossary.md) — every acronym you will hit

---

## Prerequisites for the labs

You need a Linux box (Ubuntu 22.04+ or Fedora 39+ recommended) with:

```bash
sudo apt install build-essential gdb clang lld llvm \
                 linux-headers-$(uname -r) \
                 git make bison flex libssl-dev libelf-dev \
                 qemu-system-x86 qemu-utils \
                 libdrm-dev mesa-utils \
                 strace ltrace perf trace-cmd
```

For amdgpu-specific work you also want:

```bash
sudo apt install libdrm-amdgpu1 libdrm-dev rocm-smi-lib
```

---

## How this book differs from LDD3 and from random tutorials

- **LDD3** (Linux Device Drivers, 3rd ed., Corbet/Rubini/Kroah-Hartman) is the canonical reference but it stops at kernel ~2.6.10 (2005). Many APIs have changed: `cdev_init`, `class_create`, `device_create`, `dma-buf`, `iommu`, the DRM/GEM/atomic-KMS stack — all post-LDD3. We rebuild on a modern (≥6.x) kernel.
- **Random tutorials** teach you `hello_world.ko` and stop. We go all the way to *"why does `amdgpu_cs_ioctl` need a `drm_sched_job` and a `dma_fence`?"*.
- **This book** is opinionated about what is real-world for a junior at AMD: *PCIe, DMA, ring buffers, fences, TTM, KMS, reset paths, runtime PM, lockdep*. Those are the things you'll actually be debugging in your first year.

---

## A note on the path

If you are currently a contractor and want FTE: the people who convert are the ones who:

1. **Ship code that lands upstream**, even if tiny. (One real `Signed-off-by` in `drivers/gpu/drm/amd/` from you is worth ten LeetCode mediums.)
2. **Debug something nobody else can.** Pick the ugliest open bug nobody is touching and become the expert.
3. **Are visibly competent in code review.** Read patches on `amd-gfx@lists.freedesktop.org`, ask intelligent questions.
4. **Know the hardware.** Read RDNA / CDNA ISA documents. Know what an SDMA queue is.
5. **Don't need hand-holding.** Get the kernel to build, get amdgpu to load, and triage your own bugs before asking for help.

This book is engineered to make all five of those true for you.

Let's begin.
