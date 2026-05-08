# USP 11 — /proc, /sys, /dev — the magic filesystems

> Linux exposes the kernel as files. Read them; sometimes write them.

## /proc

A virtual filesystem that gives userspace a view of kernel state. Each process has a directory `/proc/<pid>/`. Plus system-wide files at `/proc/...`.

### Per-process files

| Path | What |
|---|---|
| `/proc/PID/cmdline` | argv (NUL-separated) |
| `/proc/PID/exe` | symlink to executable |
| `/proc/PID/cwd` | symlink to current working dir |
| `/proc/PID/fd/N` | symlink for fd N |
| `/proc/PID/maps` | virtual memory map |
| `/proc/PID/status` | human-readable status (state, RSS, threads) |
| `/proc/PID/stat` | machine-readable stats |
| `/proc/PID/io` | bytes read/written |
| `/proc/PID/limits` | rlimit values |
| `/proc/PID/environ` | environment variables (NUL-separated) |
| `/proc/PID/comm` | short process name |
| `/proc/PID/task/TID/` | per-thread info |

```bash
$ cat /proc/$$/maps        # shell's address space
$ cat /proc/$$/status      # readable status
$ ls -l /proc/$$/fd        # open fds
```

### System-wide

| Path | What |
|---|---|
| `/proc/cpuinfo` | CPU details |
| `/proc/meminfo` | memory totals/free/buffers |
| `/proc/loadavg` | load averages |
| `/proc/uptime` | seconds since boot |
| `/proc/version` | kernel build string |
| `/proc/cmdline` | kernel command line |
| `/proc/modules` | loaded modules |
| `/proc/devices` | character/block devices |
| `/proc/net/tcp` | TCP sockets |
| `/proc/net/dev` | per-interface stats |
| `/proc/sys/...` | sysctl tunables |
| `/proc/interrupts` | per-IRQ counters |
| `/proc/stat` | system-wide stats |
| `/proc/buddyinfo` | per-zone free pages |
| `/proc/slabinfo` | slab allocator stats |

### sysctl — write to tune

```bash
echo 1 > /proc/sys/net/ipv4/ip_forward         # enable forwarding
sysctl net.ipv4.tcp_keepalive_time              # query
sysctl -w net.ipv4.tcp_keepalive_time=60        # set
```

`/etc/sysctl.conf` for persistent settings.

## /sys (sysfs)

A second virtual filesystem, primarily for **device** info. Mount point `/sys`.

```
/sys/class/        — by device class (net, block, dri, etc.)
/sys/bus/          — by bus (pci, usb, platform)
/sys/devices/      — actual device topology
/sys/module/       — per-module
/sys/kernel/       — kernel core
```

Examples:
```bash
$ cat /sys/class/dma_buf/                    # dma-buf class
$ cat /sys/class/drm/card0/device/vendor     # GPU vendor (e.g., 0x1002 = AMD)
$ cat /sys/class/net/eth0/operstate
$ cat /sys/block/sda/queue/scheduler         # I/O scheduler
$ echo bfq > /sys/block/sda/queue/scheduler  # set
```

For drivers, sysfs is **how user-visible attributes are exposed**. Your `amdgpu_*` debugfs/sysfs entries (e.g., `pp_dpm_sclk` to read GPU clocks) live here.

When you write a driver chapter (Part VI), you'll create your own sysfs files via `device_create_file` or `DEVICE_ATTR`.

## /dev

Device nodes. Each entry is **major:minor** numbers identifying a kernel driver (we'll see this in Part V/VI).

```bash
$ ls -l /dev/null /dev/zero /dev/random /dev/dri/card0 /dev/sda
crw-rw-rw- 1 root root 1, 3 ... /dev/null
crw-rw-rw- 1 root root 1, 5 ... /dev/zero
crw-rw-rw- 1 root root 1, 8 ... /dev/random
crw-rw---- 1 root render 226, 0 ... /dev/dri/card0
brw-rw---- 1 root disk 8, 0 ... /dev/sda
```

`c` = character device, `b` = block device.

When you `open("/dev/dri/card0", ...)`, the kernel routes to the driver registered with major 226. `read`, `write`, `ioctl`, `mmap` on that fd land in the driver's file_operations.

Key device families:
- `/dev/sd*`, `/dev/nvme*` — block devices.
- `/dev/tty*`, `/dev/pts/*` — terminals.
- `/dev/null`, `/dev/zero`, `/dev/random` — pseudo devices.
- `/dev/input/*` — keyboard, mouse, gamepad.
- `/dev/dri/card*`, `/dev/dri/renderD*` — GPU.
- `/dev/kfd` — AMD compute (KFD).
- `/dev/snd/*` — ALSA audio.

## /dev/shm

POSIX shared memory backing. `shm_open("/foo", ...)` actually creates `/dev/shm/foo`.

## /dev/pts

Pseudo-terminals for SSH/screen/tmux sessions. Each terminal gets a `/dev/pts/N`.

## /dev/loop, mounting images

`losetup /dev/loop0 image.img` then `mount /dev/loop0 /mnt` lets you mount a disk image as a filesystem. Useful for running a kernel test from an image without writing to disk.

## procfs limits

- Lots of `/proc/*` and `/sys/*` files don't have a meaningful "size" — `stat` shows 0. Read them; size is whatever bytes you get.
- They're not seekable (or seekable in odd ways).
- Cannot mmap.
- Don't expect O(1) reads — `/proc/PID/maps` for a big process is regenerated on each read.

## Kernel-style attributes (preview)

When you write your own driver later, you'll expose info via:
- **sysfs**: small, named, single-value attributes (`/sys/class/.../foo`). Use for exposed configuration.
- **debugfs**: `/sys/kernel/debug/...`. Free-form. Used for debugging-only info.
- **procfs**: legacy; new code uses sysfs/debugfs.
- **ioctls**: structured commands; opaque to filesystem.

## Container-aware /proc

Inside a container, `/proc` shows only the container's view (e.g., its own PIDs). This is done by the **PID namespace**. Some tools (htop, ps) read `/proc` and "just work" inside containers.

## Tools that walk these filesystems

- `ps` — reads `/proc/*`.
- `top`, `htop` — read `/proc/*`.
- `lsof` — reads `/proc/*/fd`.
- `pmap` — reads `/proc/PID/maps`.
- `lscpu` — reads `/sys/devices/system/cpu/`.
- `lsblk` — reads `/sys/block/`.
- `lspci` — reads `/sys/bus/pci/devices/`.
- `udev` (systemd) — reacts to events from `/sys`.

You'll be reading these files for the rest of your career.

## Common bugs

1. Trying to **mmap** /proc files — fails for most.
2. Assuming **stable size** — many entries report size 0.
3. Parsing brittle **`/proc/stat`** layout — it's column-positional and field counts can change between kernel versions.
4. Reading non-atomic state — between two reads, things may change. Use `/proc/[pid]/status` carefully.

## Try it

1. Read `/proc/self/maps` and parse it. List heap, stack, .text.
2. Read `/proc/cpuinfo`. Print only model name and cache size for each CPU.
3. Read `/sys/class/net/*/operstate` for every interface.
4. Find your GPU: `cat /sys/class/drm/card0/device/vendor` (0x1002 = AMD, 0x10de = NVIDIA, 0x8086 = Intel).
5. Watch `/proc/interrupts` over a `for` loop in shell; correlate with activity (move mouse, run a `dd`).
6. Read `/proc/[pid]/io` for a process. Understand bytes_read vs bytes_written. Compare to `/proc/[pid]/stat`.

## Read deeper

- `man 5 proc` — exhaustive catalog of /proc.
- TLPI chapter 12 (system info).
- `Documentation/filesystems/proc.rst` and `sysfs.rst` in the kernel.

Next → [Userspace performance & debug tools](USP-12-perf-tools.md).
