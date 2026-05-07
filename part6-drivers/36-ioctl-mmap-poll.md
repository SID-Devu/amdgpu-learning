# Chapter 36 — `ioctl`, `mmap`, `poll`, async I/O

`read`/`write` are not enough for real devices. Drivers expose control via `ioctl`, expose memory via `mmap`, expose readiness via `poll`. DRM does all three. This chapter covers them.

## 36.1 `ioctl` — control commands

Userspace:

```c
int ret = ioctl(fd, MYDRV_GET_STATE, &state);
```

Kernel:

```c
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg);
```

Place in `file_operations.unlocked_ioctl` (and `compat_ioctl` for 32-bit-on-64-bit).

### Encoding the command

`<linux/ioctl.h>` provides macros:

```c
#define MYDRV_IOC_MAGIC 'm'

#define MYDRV_NOP        _IO(MYDRV_IOC_MAGIC, 0)
#define MYDRV_SET_FOO    _IOW(MYDRV_IOC_MAGIC, 1, struct mydrv_foo)
#define MYDRV_GET_BAR    _IOR(MYDRV_IOC_MAGIC, 2, struct mydrv_bar)
#define MYDRV_RW_QUUX    _IOWR(MYDRV_IOC_MAGIC, 3, struct mydrv_quux)
```

`_IOW` (write to driver, i.e. user→kernel), `_IOR` (read from driver, kernel→user), `_IOWR` (both). The macros encode magic byte, command number, and direction/size — so the kernel can validate.

### A safe ioctl handler

```c
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct mydev *d = f->private_data;
    void __user *uarg = (void __user *)arg;

    if (_IOC_TYPE(cmd) != MYDRV_IOC_MAGIC)
        return -ENOTTY;

    switch (cmd) {
    case MYDRV_GET_BAR: {
        struct mydrv_bar bar;
        bar.value = atomic_read(&d->state);
        if (copy_to_user(uarg, &bar, sizeof(bar))) return -EFAULT;
        return 0;
    }
    case MYDRV_SET_FOO: {
        struct mydrv_foo foo;
        if (copy_from_user(&foo, uarg, sizeof(foo))) return -EFAULT;
        return apply_foo(d, &foo);
    }
    default:
        return -ENOTTY;
    }
}
```

Always:

- copy_from_user / copy_to_user for the entire struct.
- validate before use.
- return `-ENOTTY` for unknown commands.

DRM extends this with a registration table where each ioctl has a flag bit (DRM_RENDER_ALLOW, DRM_AUTH, DRM_MASTER) and a struct size; the core copies in/out automatically and dispatches.

## 36.2 `mmap`

User:

```c
void *p = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, offset);
```

Kernel `file_operations.mmap`:

```c
static int my_mmap(struct file *f, struct vm_area_struct *vma);
```

You must populate the VMA so that future accesses get the right physical pages. Two approaches:

### Approach A: pre-populate with `remap_pfn_range`

For a contiguous physical buffer (e.g. `dma_alloc_coherent` memory):

```c
static int my_mmap(struct file *f, struct vm_area_struct *vma)
{
    struct mydev *d = f->private_data;
    unsigned long pfn = virt_to_phys(d->dma_buf) >> PAGE_SHIFT;
    unsigned long size = vma->vm_end - vma->vm_start;

    if (size > d->dma_size) return -EINVAL;

    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    if (remap_pfn_range(vma, vma->vm_start, pfn,
                        size, vma->vm_page_prot))
        return -EAGAIN;
    return 0;
}
```

`remap_pfn_range` walks the VMA and writes PTEs pointing to the contiguous physical pages.

### Approach B: demand-fault with `vm_ops->fault`

For a buffer that's not contiguous, or whose pages should be allocated lazily:

```c
static vm_fault_t my_fault(struct vm_fault *vmf)
{
    struct mydev *d = vmf->vma->vm_private_data;
    pgoff_t pgoff = vmf->pgoff;
    struct page *page = d->pages[pgoff];

    get_page(page);
    vmf->page = page;
    return 0;
}

static const struct vm_operations_struct my_vm_ops = {
    .fault = my_fault,
};

static int my_mmap(struct file *f, struct vm_area_struct *vma)
{
    struct mydev *d = f->private_data;
    vma->vm_private_data = d;
    vma->vm_ops = &my_vm_ops;
    return 0;
}
```

When user accesses a page, the CPU faults; kernel calls `my_fault`; we return the page; kernel installs the PTE.

GEM (Chapter 45) uses this pattern: GPU buffers may be in VRAM, GTT, or system memory; the fault handler decides what to install.

### Cache attributes

```c
vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
```

For GPU command buffers and frame buffers being written by CPU and read by GPU, write-combining is correct: the CPU coalesces writes into 64-byte buffers and flushes to the device.

For doorbells and MMIO, uncached.

For system-memory buffers being read by both, the default writeback works *if* you flush carefully (`dma_sync_single_for_*`).

Choosing wrong → silent corruption.

## 36.3 `poll` / `epoll` / `select`

Userspace:

```c
struct pollfd p = { .fd = fd, .events = POLLIN };
poll(&p, 1, 1000);
```

Kernel:

```c
static __poll_t my_poll(struct file *f, struct poll_table_struct *pt)
{
    struct mydev *d = f->private_data;
    __poll_t mask = 0;

    poll_wait(f, &d->rwq, pt);
    poll_wait(f, &d->wwq, pt);

    if (available_to_read(d) > 0)  mask |= EPOLLIN  | EPOLLRDNORM;
    if (available_to_write(d) > 0) mask |= EPOLLOUT | EPOLLWRNORM;
    return mask;
}
```

`poll_wait` registers the wait-queue with the polling system without actually waiting. When the wait queue is woken (e.g. `wake_up_interruptible(&d->rwq)`), the polling system rechecks the masks.

DRM exposes `poll` for vblank events and async fence completion notifications.

## 36.4 Async I/O / `O_NONBLOCK` / signal-driven

Modern apps use **`epoll(7)`** or **`io_uring(7)`**. Drivers see them as variants of `poll` and async submission. For our purposes:

- `O_NONBLOCK` set: read/write return `-EAGAIN` if would block.
- Standard pattern: drivers expose `read`, `write`, `poll` correctly; epoll just works.
- `io_uring` integration is opt-in via `IORING_OP_*`; rarely necessary in driver code.

DRM uses fences and event fds for completion delivery, not raw async I/O.

## 36.5 Notifying user via fd

Two common patterns:

- **Read interface**: when an event happens, queue an event packet; user `read`s the packet. DRM does this for vblank, page-flip-complete.
- **eventfd**: a counting fd; the driver wakes user via `eventfd_signal()`. Simpler to integrate into existing event loops.
- **drm_syncobj**: fence-handle exposure that integrates with kernel's fence layer.

## 36.6 Compat ioctls

A 32-bit userspace can call into a 64-bit kernel. Many ioctl structs have implicit padding/alignment differences. Provide `compat_ioctl`. For pure-uint32 structs, often `compat_ptr_ioctl` suffices. DRM provides core helpers.

## 36.7 Exercises

1. Add `MYRING_GET_HEAD_TAIL` ioctl to your driver from Chapter 35; copy out a small struct.
2. Add `mmap` to expose the ring buffer's RAM page to userspace; user can read directly. (Hint: kmalloc'd memory is not contiguous physically; you may need `vmalloc_user` and `remap_vmalloc_range`.)
3. Add `poll` so `epoll` apps can wait for read/write availability.
4. Write a small userspace program that opens `/dev/myring`, mmaps, polls — and benchmark vs. `read`.
5. Read `drivers/gpu/drm/drm_ioctl.c::drm_ioctl`. Note how it dispatches by table.

---

Next: [Chapter 37 — The PCI/PCIe subsystem and writing a PCI driver](37-pci.md).
