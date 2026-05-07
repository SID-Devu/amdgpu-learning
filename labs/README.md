# Labs

Hands-on exercises with full source code. Each lab corresponds to a chapter or chapter group. They're designed so each builds on the previous.

| Lab | Chapter | What you build |
|---|---|---|
| `01-arena/` | 7 | Arena and free-list allocator in C |
| `02-spsc/` | 23 | Lock-free SPSC ring buffer in C++ |
| `03-thread-pool/` | 24 | Mutex+condvar thread pool |
| `04-hello-mod/` | 29 | First kernel module with parameters |
| `05-myring-chardev/` | 35-36 | Char driver with read/write/poll/ioctl/mmap |
| `06-edu-pci/` | 37-38 | PCI driver against QEMU's `edu` with DMA |
| `07-vkms-skeleton/` | 44-47 | Minimal DRM driver |
| `08-libdrm-amdgpu/` | 49-52 | Userspace amdgpu IB submission |

Each directory has its own `README.md`, source, and `Makefile`. Run `make` and follow.

## Prerequisites

```bash
sudo apt install build-essential gdb clang \
                 linux-headers-$(uname -r) \
                 git make qemu-system-x86 \
                 libdrm-dev mesa-utils \
                 valgrind strace
```

For amdgpu labs, you need a system with an AMD GPU and the in-tree `amdgpu` driver loaded. For kernel-module labs, any modern Linux works.

## Tips

- Always test in a VM first if you're modifying anything that talks to your GPU. `amdgpu` reset paths are mostly safe but data loss is possible.
- Keep `dmesg -w` open in a separate terminal while testing kernel modules.
- Build with `-Werror -Wall -Wextra` for userspace; turn on KASAN for kernel.
