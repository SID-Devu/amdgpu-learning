# Chapter 35 — Character drivers from scratch

A character device is the simplest driver shape and the foundation for all "byte stream" interfaces — including DRM. We build a complete one with read/write/ring buffer/wait queue, then later layer DMA, mmap, and ioctl on top.

## 35.1 Char vs. block

- **Char**: byte-stream; sequential or arbitrary access; no required block size. `/dev/null`, `/dev/zero`, `/dev/random`, `/dev/dri/card0`.
- **Block**: fixed sectors; managed by the block layer; supports read-ahead, scheduler, file systems.

DRM, evdev, V4L2, audio, GPIO — all are char.

## 35.2 The major and minor numbers

`/dev` entries have a (major, minor) pair. Major identifies the driver, minor the instance. Old kernels statically assigned majors; we now `alloc_chrdev_region` to get one dynamically.

```c
dev_t dev_no;
alloc_chrdev_region(&dev_no, 0, 1, "mydrv");   /* one minor, starting from 0 */
unregister_chrdev_region(dev_no, 1);
```

## 35.3 `cdev` — the kernel object

```c
struct cdev cdev;
cdev_init(&cdev, &my_fops);
cdev.owner = THIS_MODULE;
cdev_add(&cdev, dev_no, 1);
/* … */
cdev_del(&cdev);
```

Then create the `/dev` node via the device-class machinery so udev populates `/dev`:

```c
struct class *cls = class_create("mydrv");
device_create(cls, NULL, dev_no, NULL, "mydrv");
/* ... */
device_destroy(cls, dev_no);
class_destroy(cls);
```

(Newer kernels: `class_create("mydrv")` without `THIS_MODULE` argument.)

## 35.4 The read/write contract

The kernel calls your `read(file, ubuf, count, offp)`:

- `ubuf` is a **userspace** pointer. Do not deref directly.
- Use `copy_to_user(ubuf, kbuf, n)` — it handles page faults.
- Return number of bytes copied, or negative errno.
- `*offp` is the file offset; update if you support seeking.

Write is symmetric.

```c
static ssize_t my_read(struct file *f, char __user *ubuf,
                       size_t count, loff_t *offp)
{
    char tmp[64];
    int n;

    n = snprintf(tmp, sizeof(tmp), "value=%d\n", value);
    if (*offp >= n) return 0;             /* EOF */
    if (count > n - *offp) count = n - *offp;
    if (copy_to_user(ubuf, tmp + *offp, count))
        return -EFAULT;
    *offp += count;
    return count;
}
```

The `__user` annotation is checked by **sparse**. Your build will warn if you mix `__user` and kernel pointers.

## 35.5 A real ring-buffer chardev

We build a producer/consumer driver: writers push bytes; readers pop. Includes proper blocking, wait queues, locking.

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define BUFSZ 4096

struct mydev {
    struct cdev   cdev;
    struct mutex  lock;
    wait_queue_head_t  rwq, wwq;
    char  buf[BUFSZ];
    size_t head, tail;        /* head=write, tail=read */
};

static struct mydev *g_dev;
static dev_t           g_dev_no;
static struct class   *g_class;

static size_t available_to_read(struct mydev *d) {
    return (d->head + BUFSZ - d->tail) % BUFSZ;
}
static size_t available_to_write(struct mydev *d) {
    return BUFSZ - 1 - available_to_read(d);
}

static int my_open(struct inode *i, struct file *f) {
    f->private_data = g_dev;
    return 0;
}

static ssize_t my_read(struct file *f, char __user *ubuf,
                       size_t count, loff_t *offp)
{
    struct mydev *d = f->private_data;
    size_t avail, chunk, total = 0;
    int ret;

    if (count == 0) return 0;

    if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;

    while (available_to_read(d) == 0) {
        mutex_unlock(&d->lock);
        if (f->f_flags & O_NONBLOCK) return -EAGAIN;
        ret = wait_event_interruptible(d->rwq, available_to_read(d) > 0);
        if (ret) return ret;
        if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;
    }

    avail = available_to_read(d);
    if (count > avail) count = avail;

    while (count > 0) {
        chunk = min(count, BUFSZ - d->tail);
        if (copy_to_user(ubuf + total, d->buf + d->tail, chunk)) {
            mutex_unlock(&d->lock);
            return -EFAULT;
        }
        d->tail = (d->tail + chunk) % BUFSZ;
        total  += chunk;
        count  -= chunk;
    }

    mutex_unlock(&d->lock);
    wake_up_interruptible(&d->wwq);
    return total;
}

static ssize_t my_write(struct file *f, const char __user *ubuf,
                        size_t count, loff_t *offp)
{
    struct mydev *d = f->private_data;
    size_t avail, chunk, total = 0;
    int ret;

    if (count == 0) return 0;

    if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;

    while (available_to_write(d) == 0) {
        mutex_unlock(&d->lock);
        if (f->f_flags & O_NONBLOCK) return -EAGAIN;
        ret = wait_event_interruptible(d->wwq, available_to_write(d) > 0);
        if (ret) return ret;
        if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;
    }

    avail = available_to_write(d);
    if (count > avail) count = avail;

    while (count > 0) {
        chunk = min(count, BUFSZ - d->head);
        if (copy_from_user(d->buf + d->head, ubuf + total, chunk)) {
            mutex_unlock(&d->lock);
            return -EFAULT;
        }
        d->head = (d->head + chunk) % BUFSZ;
        total  += chunk;
        count  -= chunk;
    }

    mutex_unlock(&d->lock);
    wake_up_interruptible(&d->rwq);
    return total;
}

static const struct file_operations my_fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .read    = my_read,
    .write   = my_write,
    .llseek  = no_llseek,
};

static int __init my_init(void)
{
    int ret;

    g_dev = kzalloc(sizeof *g_dev, GFP_KERNEL);
    if (!g_dev) return -ENOMEM;
    mutex_init(&g_dev->lock);
    init_waitqueue_head(&g_dev->rwq);
    init_waitqueue_head(&g_dev->wwq);

    ret = alloc_chrdev_region(&g_dev_no, 0, 1, "myring");
    if (ret) goto err_free;

    cdev_init(&g_dev->cdev, &my_fops);
    g_dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&g_dev->cdev, g_dev_no, 1);
    if (ret) goto err_unreg;

    g_class = class_create("myring");
    if (IS_ERR(g_class)) { ret = PTR_ERR(g_class); goto err_del; }

    device_create(g_class, NULL, g_dev_no, NULL, "myring");
    pr_info("myring loaded major=%d minor=%d\n", MAJOR(g_dev_no), MINOR(g_dev_no));
    return 0;

err_del:    cdev_del(&g_dev->cdev);
err_unreg:  unregister_chrdev_region(g_dev_no, 1);
err_free:   kfree(g_dev);
    return ret;
}

static void __exit my_exit(void)
{
    device_destroy(g_class, g_dev_no);
    class_destroy(g_class);
    cdev_del(&g_dev->cdev);
    unregister_chrdev_region(g_dev_no, 1);
    kfree(g_dev);
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
```

Test:

```bash
sudo insmod myring.ko
sudo chmod 666 /dev/myring
echo "hello world" > /dev/myring
cat /dev/myring        # in another terminal blocks, then prints
```

You now have a working char driver with proper blocking I/O.

## 35.6 What we did right

- **Mutex** protecting state.
- **Two wait queues** — one for readers, one for writers; correct for producer/consumer.
- **Interruptible** waits — signals work.
- **`O_NONBLOCK`** support for `-EAGAIN`.
- **`copy_from_user`/`copy_to_user`** for userspace buffers.
- **Proper init/exit unwind**.
- **`__user`** annotation in API.

## 35.7 What's missing (next chapters)

- `poll`/`epoll` support — Chapter 36.
- `ioctl` for control — Chapter 36.
- `mmap` — Chapter 36.
- Multiple opens with shared data — Chapter 36.

## 35.8 Exercises

1. Build the driver above; test basic read/write.
2. Add `poll` (next chapter teaches; for now: `poll_wait(f, &d->rwq, p); poll_wait(f, &d->wwq, p);` and return appropriate `EPOLLIN/EPOLLOUT`).
3. Add an `ioctl` (`MYRING_GET_STATE`) returning current head/tail to userspace via a struct.
4. Run multiple readers/writers; observe correctness. Then run under TSan-like stress (loop 1M ops/thread).
5. Read `drivers/char/mem.c`; especially `read_zero` / `write_null` — minimal char drivers.

---

Next: [Chapter 36 — `ioctl`, `mmap`, polling, async I/O](36-ioctl-mmap-poll.md).
