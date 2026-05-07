# Chapter 28 — The kernel source tree and how to build it

If you can't build, boot, and modify a kernel, you can't write drivers. This chapter walks you from "no kernel checked out" to "I just booted my own bzImage with `printk("hello\n")` in `start_kernel`."

## 28.1 The tree

```bash
git clone --depth 1 \
    https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
cd linux
ls
```

The top-level dirs:

| Dir | Contents |
|---|---|
| `arch/` | per-arch code (x86, arm64, riscv, …) |
| `block/` | block layer |
| `crypto/` | kernel crypto |
| `drivers/` | every device driver — including `drivers/gpu/drm/amd/` |
| `fs/` | filesystems and VFS |
| `include/` | public kernel headers |
| `init/` | start_kernel, early boot |
| `ipc/` | sysv ipc, shm, semaphores |
| `kernel/` | scheduler, time, lockdep, RCU, signals |
| `lib/` | data structures, sort, etc. |
| `mm/` | memory management |
| `net/` | networking |
| `samples/` | example modules |
| `scripts/` | build helpers, checkpatch, etc. |
| `security/` | LSM, SELinux, AppArmor |
| `sound/` | ALSA |
| `tools/` | userspace tools (perf, bpftool) |
| `Documentation/` | hundreds of essential docs |

For our work the deep targets are:

- `drivers/gpu/drm/` (DRM core)
- `drivers/gpu/drm/scheduler/` (DRM scheduler)
- `drivers/gpu/drm/ttm/` (TTM memory manager)
- `drivers/gpu/drm/amd/amdgpu/` (~800k lines of C)
- `drivers/gpu/drm/amd/display/` (DC — display core)
- `drivers/gpu/drm/amd/pm/` (power management)
- `drivers/gpu/drm/amd/include/` (hardware register defs)

You'll spend years here.

## 28.2 Configure

Pick a starting config:

```bash
make defconfig                    # generic
make x86_64_defconfig
make olddefconfig                 # bring an old .config forward
make menuconfig                   # interactive ncurses
```

Typical recipe for development:

```bash
cp /boot/config-$(uname -r) .config
make olddefconfig
scripts/config --enable DEBUG_KERNEL \
                --enable DEBUG_INFO \
                --enable PROVE_LOCKING \
                --enable DEBUG_ATOMIC_SLEEP \
                --enable KASAN \
                --enable KASAN_GENERIC \
                --enable DEBUG_VM \
                --enable DEBUG_LIST \
                --enable DRM_AMDGPU
make olddefconfig
```

Keep amdgpu **=m** (module) if you want to load/unload during dev:

```bash
scripts/config --module DRM_AMDGPU
```

## 28.3 Build

```bash
make -j$(nproc)
```

Output:

- `vmlinux` — the kernel itself, ELF.
- `arch/x86/boot/bzImage` — compressed kernel for boot.
- `*.ko` — modules.

To install (real machine):

```bash
sudo make modules_install
sudo make install              # copies bzImage, runs update-grub
```

Then reboot. Pick the new kernel from GRUB. `uname -a` confirms.

## 28.4 Boot in a VM (faster + safer)

Strongly recommended for development. QEMU + a small rootfs:

```bash
qemu-system-x86_64 \
    -kernel arch/x86/boot/bzImage \
    -append "console=ttyS0 nokaslr" \
    -nographic \
    -enable-kvm \
    -m 4G -smp 4 \
    -initrd ../rootfs.cpio
```

Boot. You're in your kernel.

For amdgpu on QEMU, you need GPU passthrough (VFIO) which is more work. Easier path: build amdgpu, install to your real Linux box, reboot.

## 28.5 The first patch you write

Find `init/main.c::start_kernel()`. Add:

```c
asmlinkage __visible void __init __no_sanitize_address start_kernel(void)
{
    pr_info("hello from my custom kernel\n");
    /* … existing body … */
}
```

Rebuild, reboot, `dmesg | head` — your message is at the top. You have just modified the world's most-deployed C codebase.

## 28.6 Out-of-tree module

For driver development, build *your* module out-of-tree against the installed kernel:

```bash
sudo apt install linux-headers-$(uname -r)
mkdir -p /tmp/hello && cd /tmp/hello
```

`hello.c`:

```c
#include <linux/init.h>
#include <linux/module.h>

static int __init hello_init(void) { pr_info("hello, world\n"); return 0; }
static void __exit hello_exit(void) { pr_info("goodbye, world\n"); }

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("you");
MODULE_DESCRIPTION("hello world");
```

`Makefile`:

```make
obj-m := hello.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

Build and load:

```bash
make
sudo insmod hello.ko
dmesg | tail
sudo rmmod hello
dmesg | tail
```

If `dmesg` shows your messages, you've successfully:

- Written a kernel module.
- Built it against your running kernel's headers.
- Loaded and unloaded it.

The hardest barrier in driver development is past.

## 28.7 Reading the source — tools

- `cscope -R -k` then `cscope` — cross-reference jump.
- `git grep -n SYMBOL` — fast grep.
- `make tags` then vim `:tag SYMBOL`.
- LSP: `clangd` with `bear -- make -j`. Modern editors (vscode/cursor) work with the kernel.
- `kernel.org/doc/html/latest/` — official docs.
- `lwn.net/Kernel/Index/` — articles by topic.

## 28.8 Patch flow

The kernel uses git, plain mailing list patches (not GitHub PRs):

```bash
git checkout -b my-fix
# … edit …
git add -p
git commit -s -m "drivers/foo: fix bar"
git format-patch -1 -o patches/
scripts/checkpatch.pl patches/0001-*.patch
scripts/get_maintainer.pl patches/0001-*.patch
git send-email patches/0001-*.patch --to=<maintainers> --cc=<lists>
```

`-s` adds your `Signed-off-by`. `checkpatch.pl` enforces style. `get_maintainer.pl` tells you whom to email.

For amdgpu the relevant list is `amd-gfx@lists.freedesktop.org`. Subscribe.

## 28.9 Exercises

1. Clone, configure, build, install, boot a custom kernel. Verify with `uname -a`.
2. Add a `pr_info` to `start_kernel`; observe in dmesg.
3. Build the `hello.ko` from §28.6. Insmod, rmmod, observe dmesg.
4. Run `scripts/get_maintainer.pl drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c`. Note the names.
5. Skim `Documentation/process/submitting-patches.rst`. You will follow this when you submit your first amdgpu patch.

---

Next: [Chapter 29 — Loadable kernel modules — your first module](29-modules.md).
