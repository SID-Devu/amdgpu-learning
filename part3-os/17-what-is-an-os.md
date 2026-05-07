# Chapter 17 — What an OS is and the boot path on x86_64

You cannot write a driver without a deep operational picture of the OS. This chapter gives that picture for Linux on x86_64 — and the AArch64 differences where they matter for AMD/Qualcomm engineers.

## 17.1 What "operating system" means

An OS does three jobs:

1. **Multiplex hardware** — many programs share one machine.
2. **Abstract** — programs deal with files, sockets, pipes; the OS hides disks, NICs, IPC.
3. **Protect** — programs cannot trash each other or the kernel.

A modern monolithic OS like Linux runs in two modes:

- **Kernel mode** (`CPL=0` on x86, `EL1` on AArch64): full privileges, can access any memory and any I/O.
- **User mode** (`CPL=3`, `EL0`): runs your apps; access mediated by page tables and the syscall interface.

A driver runs in kernel mode. A bug in a driver can crash the machine or corrupt anything. Hence the discipline.

## 17.2 The boot path on x86_64

When you press power:

1. **Reset vector**: CPU starts in 16-bit real mode at `0xFFFFFFF0`.
2. **UEFI firmware** (or legacy BIOS) runs from flash. Initializes RAM, PCIe, integrated devices. On modern UEFI machines, the firmware is in C, sometimes from EDK2.
3. **UEFI boot manager** loads the EFI System Partition's `\EFI\BOOT\BOOTX64.EFI` (or whatever's in `BootOrder`).
4. **GRUB** (or systemd-boot) loads the **Linux kernel** (`bzImage`) and **initrd** (initial RAM disk) into memory.
5. **Kernel decompressor** (`arch/x86/boot/`) runs in 32-bit, switches to 64-bit long mode, sets up initial page tables, jumps to `start_kernel`.
6. **`start_kernel()`** in `init/main.c` initializes everything: the scheduler, memory allocator, VFS, drivers, network stack.
7. **PID 1** (`/sbin/init` or `systemd`) starts the userspace. From here, multiplexing begins.

The driver lifecycle starts somewhere around step 6: PCI devices are enumerated; their drivers are matched and probed; `amdgpu.ko` (built-in or modular) eventually binds to the GPU.

The AArch64 boot is similar (UEFI on most servers, U-Boot on phones/embedded). On Qualcomm SoCs, the boot is more elaborate (PBL → SBL → AOP → ABL → kernel, with secure-world TEE involvement). You don't need to memorize this for AMD work, but if you target Qualcomm, learn the chain.

## 17.3 The kernel's view of memory

After boot, kernel virtual address space (x86_64, 4-level paging, 48-bit virt):

```
0xffff800000000000 ─ 0xffffbfffffffffff   direct map of physical RAM
0xffffc00000000000 ─ 0xffffe7ffffffffff   vmalloc / ioremap
0xffffea0000000000                          memmap (struct page array)
0xffffffff80000000                          kernel image (text, data, bss)
0xffffffffa0000000                          module load area
```

Userspace (per process):

```
0x0000000000400000   text
0x0000000001000000   heap (grows up via brk/mmap)
0x00007ffffffff000   stack base (grows down)
```

The line at `0x0000800000000000` and `0xffff800000000000` is enforced by hardware (canonical addresses). The bottom half is user, the top is kernel — every process has the kernel mapped, but with a privilege bit that makes it inaccessible from userspace.

## 17.4 Privilege rings, CPL, syscall

x86_64 protection rings: 0 (kernel) … 3 (user). Userspace runs at CPL=3. To enter the kernel:

- **`syscall`** instruction — by convention, syscall number in `rax`, args in `rdi, rsi, rdx, r10, r8, r9`.
- **Interrupt** — `int 0x80` legacy, or hardware IRQs.
- **Exception** — page fault, GPF, etc.

The CPU saves user state (RIP, RSP, RFLAGS), switches stacks (`%gs:cpu_current_top_of_stack`), and jumps to a fixed entry point set in `MSR_LSTAR`. The kernel runs the syscall handler, returns with `sysret`.

Page tables protect kernel memory: pages with the U/S bit clear are unreachable from CPL=3.

## 17.5 The big abstractions

These are what userspace sees:

- **Process** — `fork()` creates one; `exec()` replaces the image; `wait()` reaps.
- **Thread** — Linux models them as processes that share an address space; `clone(CLONE_VM | CLONE_FS | …)` is the primitive; `pthread_create` is glibc's wrapper.
- **File descriptor** — small integer that indexes into a per-process table of `struct file` pointers. `open`, `read`, `write`, `close`. **Almost everything is a file**: regular files, pipes, sockets, devices, GPUs (yes, `/dev/dri/card0` is a file).
- **Memory mapping** — `mmap` puts file content (or anonymous pages) into your VM. Used for shared memory, large allocations, and **GPU buffer sharing**.
- **Signals** — async notifications (SIGINT, SIGSEGV). Driver authors mostly ignore signals; userspace runtime authors must handle them.
- **Pipes / sockets** — IPC primitives.

The driver writer ultimately lives behind the *file descriptor* abstraction. A user opens `/dev/dri/card0`, gets an fd, calls `ioctl`s on it. We'll spend many chapters on those ioctls.

## 17.6 Modes of running drivers

- **Built-in**: compiled into `vmlinux`. Loaded at boot.
- **Module**: compiled as `*.ko`, loaded on demand by `modprobe` or by `udev` matching a device.
- **Out-of-tree module**: built against installed headers; common during development.

Modular vs. built-in is mostly an organizational choice. Behavior is the same.

## 17.7 The kernel-userspace contract: stable APIs

- The **syscall interface** is stable forever.
- The **/proc and /sys interfaces** are stable forever once exposed.
- **Kernel internal APIs** are NOT stable. They change every release. (This is intentional: contributors must update in-tree drivers when they change a core API; out-of-tree drivers must keep up.)
- **DRM ioctls** are stable forever once added. Adding a new one is a multi-list-review process.

Therefore: every `ioctl` you add to amdgpu is a forever commitment. Add them carefully and version them.

## 17.8 The major subsystems we will touch

```
                       ┌────────────────────────────────────────┐
                       │     User space (apps, libdrm, mesa)    │
                       └────────────────────────────────────────┘
                                       │ syscalls
   ┌───────────────────────────────────┼────────────────────────────────┐
   │                          Linux kernel                              │
   │                                   │                                │
   │   VFS ── Page cache ── Block ── Net ── DRM ── Sound ── USB ── …    │
   │                                   │                                │
   │           ┌───────── Driver model (kobject, sysfs) ─────────┐      │
   │           │            ↓              ↓             ↓       │      │
   │           │       PCI bus     Platform bus     USB bus      │      │
   │           └────────────────────────────────────────────────┘      │
   │                                   │                                │
   │   Memory mgmt — Scheduler — IRQ — Locks — Kthreads — Workqueues    │
   └────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
                                Hardware
```

Driver work touches almost every box. GPU work additionally touches `dma-buf` (cross-driver buffer sharing), `iommu`, `pci`, `drm/*`, and `power/runtime PM`.

## 17.9 Exercises

1. `strace ls` — observe every syscall. Identify `execve`, `mmap`, `openat`, `read`, `write`, `close`.
2. `cat /proc/cpuinfo`, `/proc/meminfo`, `/proc/interrupts`, `/proc/modules`. Read each.
3. Run `dmesg | head -200` after boot. Identify the lines from your storage controller, your network card, and (if AMD) `[drm] amdgpu`.
4. `lsmod | head` — count loaded modules. `modinfo nvme` and read.
5. Find your AMD GPU in sysfs: `ls /sys/class/drm/`. Walk the directory tree.

---

Next: [Chapter 18 — Processes, threads, address spaces, syscalls](18-processes-threads-syscalls.md).
