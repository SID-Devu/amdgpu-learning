# Chapter 21 — The I/O stack: VFS, page cache, block layer

For a GPU driver writer this chapter is short — your driver isn't a filesystem. But you must know the shape of the stack because you live above the **VFS** abstraction (you implement `file_operations`), and because GPU buffers occasionally cross the line into mmap-able files via `dma-buf` and `udmabuf`.

## 21.1 The VFS

The **Virtual File System** is the kernel's polymorphic interface to "things you can read/write." Every file-like object — disk file, pipe, socket, device, GPU — is a `struct file` whose `f_op` is a `struct file_operations`. The kernel implements `read(2)` / `write(2)` / `mmap(2)` by dispatching through `f_op`.

```c
struct file {
    struct path                 f_path;
    struct inode               *f_inode;
    const struct file_operations *f_op;
    void                       *private_data;     /* driver's per-fd state */
    fmode_t                     f_mode;
    loff_t                      f_pos;
    /* … */
};
```

When userspace does `open("/dev/dri/card0", O_RDWR)`, the VFS:

1. Resolves the path; finds an inode that says "char device, major 226, minor 0."
2. Looks up the registered char-device class for that major; gets `drm_fops` (DRM's `file_operations`).
3. Creates a `struct file`, sets `f_op = &drm_fops`, calls `drm_fops.open`.
4. The DRM open allocates a per-file structure and stores it in `file->private_data`.

From this point, `read(fd, …)` calls `drm_fops.read` — DRM's read. `ioctl(fd, …)` calls `drm_fops.unlocked_ioctl`. The driver's per-fd state is in `file->private_data`.

**Implementing a driver = implementing a `file_operations`.** Internalize this.

## 21.2 The page cache

For real files on disk, the kernel maintains a **page cache**: in-RAM copies of file content keyed by `(inode, offset)`. Reads read from the cache (or fault them in from disk). Writes update the cache and mark dirty; a writeback path eventually flushes to disk.

`mmap` on a file uses the page cache directly: the user's pages *are* the page cache pages. This is why `mmap`+`read` of the same file shares data automatically.

For drivers, the page cache is mostly invisible. But:

- Some drivers expose pseudo-files in `debugfs`/`sysfs`. Those don't use the page cache.
- `dma-buf` and `udmabuf` create file-backed mmap-able objects with custom `vm_ops->fault`. The kernel page-cache machinery touches them (briefly).

## 21.3 Block layer (briefly)

Below the filesystem is the **block layer**: requests to read/write 4 KiB sectors. Modern: **blk-mq** (multi-queue block layer). Drivers register a `blk_mq_ops` and a request queue; the layer delivers requests to be dispatched to hardware.

NVMe drivers, SCSI, virtio-blk all live here. amdgpu does not. Skip unless you join the storage team.

## 21.4 Network stack (briefer)

Sockets → protocol stack (TCP/IP) → driver. A NIC driver registers with `netdev`. amdgpu does not implement netdev. Skip.

## 21.5 The `inode` and the `dentry`

`struct inode` is the kernel's view of a file (size, perms, owner, link count). `struct dentry` is the path-lookup cache (one entry per path component). For drivers, you mostly don't allocate inodes (the device-class machinery does). But understand: `device_create()` plants an inode in `/dev`.

## 21.6 sysfs and debugfs

These are *kernel virtual filesystems* — not backed by disk, but by callback functions in the kernel.

- **sysfs (`/sys`)**: structured tree of devices, classes, drivers. Each entry corresponds to a `kobject`. Drivers expose attributes via macros like `DEVICE_ATTR_RW(name)` and a show/store pair.
- **debugfs (`/sys/kernel/debug`)**: ad-hoc developer files. amdgpu uses heavily for debugging registers, ring queues, fences. Drivers register a debugfs node with `debugfs_create_file()` and a `file_operations`.

For a driver, sysfs is the user-visible knob; debugfs is the developer's window.

## 21.7 `procfs` (`/proc`)

Older sibling of sysfs. Drivers shouldn't add new `/proc` entries — use sysfs/debugfs. But you'll read `/proc/interrupts`, `/proc/iomem`, `/proc/meminfo` constantly.

## 21.8 The driver-author's view of the I/O stack

Every driver is a **producer of `file_operations`** and a **consumer of bus, IRQ, DMA, sysfs, devm APIs**:

```
              ┌─────────────────────────┐
              │   open / read / mmap    │   userspace
              └────────────┬────────────┘
                           │ syscall
              ┌────────────▼────────────┐
              │           VFS           │
              └────────────┬────────────┘
                           │ f_op->read, etc.
              ┌────────────▼────────────┐
              │ Char/DRM driver         │   <-- you implement this
              │  - parses request       │
              │  - touches MMIO         │
              │  - schedules DMA        │
              │  - waits on IRQ/fence   │
              └────────────┬────────────┘
                           ▼
                       Hardware
```

Everything else in this textbook is preparation to write that middle box well.

## 21.9 Exercises

1. `cat /proc/devices` — see registered char/block majors.
2. Read `Documentation/filesystems/vfs.rst` (a long, dry but excellent doc).
3. List your DRM device tree under `/sys/class/drm/`. Walk `card0/device/` (a symlink into `/sys/devices/pci…/`). Identify `vendor`, `device`, `boot_vga`, `power/`.
4. Run `mount | grep debugfs`; if it isn't mounted, mount it: `sudo mount -t debugfs none /sys/kernel/debug`. List `/sys/kernel/debug/dri/0/`.
5. Read three files in `/sys/kernel/debug/dri/0/`: `amdgpu_pm_info`, `amdgpu_gpu_recover`, `name`. We will revisit these.

---

Next: [Chapter 22 — Interrupts, exceptions, IRQ delivery](22-interrupts.md).
