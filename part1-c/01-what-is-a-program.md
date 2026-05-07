# Chapter 1 — What is a program, a compiler, and an operating system?

Before we write a line of C, you must have a correct mental model of what happens when you run a program. If this model is wrong, every later chapter (memory, drivers, GPU command submission) will be confusing. If it is right, everything else falls into place.

## 1.1 The machine

A modern PC has, at minimum:

- One or more **CPUs**, each with several **cores**, each able to run one (or, with SMT, two) **hardware threads** at once. Each core has private **L1** and **L2** caches; cores share an **L3** cache.
- **DRAM** (main memory): large (GBs), slow compared to caches (~100 ns vs ~1 ns for L1).
- **MMIO devices**: GPU, NIC, NVMe, USB controller, sound. Each appears as a region of physical address space; reading/writing those addresses talks to the device.
- A **PCIe** fabric connecting the CPU to those devices.
- A **boot ROM** (UEFI/BIOS firmware) that runs on power-on.
- A **storage device** (NVMe SSD) that holds the OS and your files.

Every program ultimately runs on this machine. The job of the OS is to *give every program the illusion that it owns the machine alone*.

## 1.2 What a CPU actually does

A CPU is, at its core, a loop:

```
while (true) {
    instruction = MEMORY[program_counter];
    decode(instruction);
    execute(instruction);
    program_counter += instruction_size;  // or branch target
}
```

That loop runs at a few GHz. Each instruction is a few bytes of binary. The CPU has a small set of **registers** (`rax`, `rbx`, `rsp`, `rip`, etc. on x86_64; `x0`–`x30`, `sp`, `pc` on AArch64) that it operates on. To do anything with main memory it must `LOAD` a value into a register, operate on it, then `STORE` it back.

A "program" is a sequence of those instructions plus initial data. That's it. The whole stack — your editor, Chrome, Linux, amdgpu — is just very long sequences of those instructions running on cores.

## 1.3 What a compiler does

You don't want to write `LOAD r0, [0x4000]; ADD r0, r1, r0; STORE [0x4004], r0`. You want to write `c = a + b;`.

A **compiler** translates a high-level language (C, C++, Rust) into machine instructions. The classical pipeline:

```
source.c  ──[preprocessor]──>  expanded.c
expanded.c  ──[compiler]──>    source.s        (assembly)
source.s    ──[assembler]──>   source.o        (object file: machine code + symbols)
*.o + libs  ──[linker]──>      a.out / vmlinux (executable)
```

GCC and Clang both do all four steps. We'll use both. Try this:

```bash
echo 'int main(void){return 1+2;}' > t.c
gcc -O2 -S t.c -o -
```

You will see something like:

```
main:
    mov     eax, 3
    ret
```

The compiler **constant-folded** `1+2` into `3` and emitted exactly two instructions. This is your first hint that the compiler is far cleverer than any code you write. You will spend much of your career either *helping it* (by writing clear code) or *fighting it* (when it optimizes away something you wanted to keep — relevant for memory-mapped I/O).

## 1.4 What an operating system does

When you run `./a.out`, the OS:

1. **Reads the ELF file** from disk. It contains code, data, and metadata.
2. **Creates a process**: a new address space (a virtual memory map), a kernel data structure (`struct task_struct` in Linux), file descriptor table, etc.
3. **Maps the program's segments** into that address space (`.text` for code, `.data` for initialized globals, `.bss` for zero-initialized globals, plus a stack and heap).
4. **Sets the program counter** to the entry point and **jumps**. Now your code runs.
5. When your code wants something only the kernel can do (open a file, allocate memory from the OS, talk to a device) it issues a **system call** — on x86_64, the `syscall` instruction; on ARM64, `svc #0`. The CPU traps into kernel mode, the kernel does the work, and returns.

Two crucial mental concepts come from this:

- **User mode vs. kernel mode**: a CPU bit decides whether the currently executing code can do privileged things (talk to hardware, change page tables). Your `a.out` runs in user mode. The kernel runs in kernel mode. A driver runs in kernel mode. **Crashing in kernel mode crashes the machine.**
- **Virtual memory**: every process sees a clean, flat address space starting at low addresses. Two processes can both have a pointer `0x4000`, and they refer to *different* physical bytes. The MMU (a hardware unit in the CPU) translates virtual to physical on every memory access using **page tables** maintained by the kernel.

We will return to both ideas many times.

## 1.5 Where a device driver lives

A **device driver** is code that runs *in kernel mode* and bridges the gap between the OS and a piece of hardware. Examples:

- `e1000e.ko` — Intel gigabit NIC driver.
- `nvme.ko` — NVMe SSD driver.
- `amdgpu.ko` — AMD GPU driver. ~800k lines of C.

Drivers don't usually run as their own process. They're modules linked into the kernel. They expose interfaces to userspace (files in `/dev`, `ioctl`s, `sysfs` attributes, `mmap`able regions) and they talk to the hardware via MMIO, DMA, and interrupts.

Diagram:

```
┌──────────────────────────────────────┐
│      User process (your app)         │
│   reads /dev/dri/card0, calls ioctl  │
└──────────────────────────────────────┘
              │  syscall
              ▼
┌──────────────────────────────────────┐
│           Linux kernel               │
│  ┌──────────────────────────────┐    │
│  │ VFS / DRM core / TTM         │    │
│  │ amdgpu driver (this is us)   │    │
│  └──────────────────────────────┘    │
└──────────────────────────────────────┘
              │  MMIO / DMA / IRQ
              ▼
┌──────────────────────────────────────┐
│           AMD GPU                    │
└──────────────────────────────────────┘
```

Everything in this book is preparation to confidently write and modify the middle box.

## 1.6 The languages of the trade

- **C** — the language of the Linux kernel and of every driver in `drivers/gpu/drm/amd/amdgpu/`. **Required.**
- **C++** — used in userspace (Mesa, ROCm runtime, ROCclr, HIP). **Required for AMD GPU work.**
- **Assembly** — you don't write much, but you must read x86_64 and AArch64 assembly when debugging.
- **Python** — kernel build scripts, test harnesses, ROCm tools.
- **Rust** — being introduced into the kernel; learn it after you've mastered C.
- **GLSL / SPIR-V / GCN / RDNA ISA** — GPU shader languages and AMD's GPU instruction sets. Read the ISA manuals once you reach Part VII.

Don't try to learn all of these at once. C → kernel → C++ → GPU → Rust is a sane order, and it's the order this book follows.

## 1.7 Mental model checklist

Before you turn the page, answer aloud:

1. What is the difference between a process and a thread?
2. What does the CPU do at every clock tick?
3. What is the difference between virtual and physical memory?
4. Why can't a normal program write to a register on the GPU directly?
5. Why does writing to a hardware register require special care from the compiler?

If any answer is fuzzy, re-read the matching section. The whole book sits on top of these ideas.

---

Next: [Chapter 2 — Your first C programs and the toolchain](02-toolchain-and-first-programs.md).
