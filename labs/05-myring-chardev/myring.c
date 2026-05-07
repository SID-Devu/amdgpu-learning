/*
 * myring: a producer/consumer ring buffer exposed as a character device.
 *
 * Demonstrates: cdev, file_operations (read, write, poll, mmap, ioctl),
 *               wait queues, mutex, signal-safe waiting.
 *
 * Lab 05 of the Linux Driver Textbook.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define DEVNAME "myring"
#define BUFSZ   4096

#define MYRING_IOC_MAGIC 'r'
struct myring_state {
    __u32 head;
    __u32 tail;
    __u32 capacity;
    __u32 _reserved;
};
#define MYRING_GET_STATE  _IOR(MYRING_IOC_MAGIC, 0, struct myring_state)
#define MYRING_RESET      _IO (MYRING_IOC_MAGIC, 1)

struct mydev {
    struct cdev          cdev;
    struct mutex         lock;
    wait_queue_head_t    rwq, wwq;
    char                 buf[BUFSZ];
    size_t               head, tail;
};

static struct mydev *g_dev;
static dev_t           g_dev_no;
static struct class   *g_class;

static size_t avail_read(struct mydev *d)  { return (d->head + BUFSZ - d->tail) % BUFSZ; }
static size_t avail_write(struct mydev *d) { return BUFSZ - 1 - avail_read(d); }

static int my_open(struct inode *i, struct file *f)
{
    f->private_data = container_of(i->i_cdev, struct mydev, cdev);
    return 0;
}

static int my_release(struct inode *i, struct file *f)
{
    return 0;
}

static ssize_t my_read(struct file *f, char __user *ubuf,
                       size_t count, loff_t *off)
{
    struct mydev *d = f->private_data;
    size_t avail, chunk, total = 0;
    int ret;

    if (!count) return 0;

    if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;

    while (avail_read(d) == 0) {
        mutex_unlock(&d->lock);
        if (f->f_flags & O_NONBLOCK) return -EAGAIN;
        ret = wait_event_interruptible(d->rwq, avail_read(d) > 0);
        if (ret) return ret;
        if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;
    }

    avail = avail_read(d);
    if (count > avail) count = avail;

    while (count) {
        chunk = min(count, BUFSZ - d->tail);
        if (copy_to_user(ubuf + total, d->buf + d->tail, chunk)) {
            mutex_unlock(&d->lock);
            return -EFAULT;
        }
        d->tail = (d->tail + chunk) % BUFSZ;
        total += chunk;
        count -= chunk;
    }

    mutex_unlock(&d->lock);
    wake_up_interruptible(&d->wwq);
    return total;
}

static ssize_t my_write(struct file *f, const char __user *ubuf,
                        size_t count, loff_t *off)
{
    struct mydev *d = f->private_data;
    size_t avail, chunk, total = 0;
    int ret;

    if (!count) return 0;

    if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;

    while (avail_write(d) == 0) {
        mutex_unlock(&d->lock);
        if (f->f_flags & O_NONBLOCK) return -EAGAIN;
        ret = wait_event_interruptible(d->wwq, avail_write(d) > 0);
        if (ret) return ret;
        if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;
    }

    avail = avail_write(d);
    if (count > avail) count = avail;

    while (count) {
        chunk = min(count, BUFSZ - d->head);
        if (copy_from_user(d->buf + d->head, ubuf + total, chunk)) {
            mutex_unlock(&d->lock);
            return -EFAULT;
        }
        d->head = (d->head + chunk) % BUFSZ;
        total += chunk;
        count -= chunk;
    }

    mutex_unlock(&d->lock);
    wake_up_interruptible(&d->rwq);
    return total;
}

static __poll_t my_poll(struct file *f, struct poll_table_struct *pt)
{
    struct mydev *d = f->private_data;
    __poll_t mask = 0;

    poll_wait(f, &d->rwq, pt);
    poll_wait(f, &d->wwq, pt);

    if (avail_read(d) > 0)  mask |= EPOLLIN  | EPOLLRDNORM;
    if (avail_write(d) > 0) mask |= EPOLLOUT | EPOLLWRNORM;
    return mask;
}

static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct mydev *d = f->private_data;
    void __user *u = (void __user *)arg;

    if (_IOC_TYPE(cmd) != MYRING_IOC_MAGIC) return -ENOTTY;

    switch (cmd) {
    case MYRING_GET_STATE: {
        struct myring_state st;
        if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;
        st.head     = d->head;
        st.tail     = d->tail;
        st.capacity = BUFSZ;
        st._reserved = 0;
        mutex_unlock(&d->lock);
        if (copy_to_user(u, &st, sizeof(st))) return -EFAULT;
        return 0;
    }
    case MYRING_RESET:
        if (mutex_lock_interruptible(&d->lock)) return -ERESTARTSYS;
        d->head = d->tail = 0;
        mutex_unlock(&d->lock);
        wake_up_interruptible(&d->wwq);
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct file_operations my_fops = {
    .owner          = THIS_MODULE,
    .open           = my_open,
    .release        = my_release,
    .read           = my_read,
    .write          = my_write,
    .poll           = my_poll,
    .unlocked_ioctl = my_ioctl,
    .compat_ioctl   = my_ioctl,
    .llseek         = no_llseek,
};

static int __init my_init(void)
{
    int ret;

    g_dev = kzalloc(sizeof(*g_dev), GFP_KERNEL);
    if (!g_dev) return -ENOMEM;
    mutex_init(&g_dev->lock);
    init_waitqueue_head(&g_dev->rwq);
    init_waitqueue_head(&g_dev->wwq);

    ret = alloc_chrdev_region(&g_dev_no, 0, 1, DEVNAME);
    if (ret) goto err_free;

    cdev_init(&g_dev->cdev, &my_fops);
    g_dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&g_dev->cdev, g_dev_no, 1);
    if (ret) goto err_unreg;

    g_class = class_create(DEVNAME);
    if (IS_ERR(g_class)) { ret = PTR_ERR(g_class); goto err_del; }

    device_create(g_class, NULL, g_dev_no, NULL, DEVNAME);
    pr_info("%s: registered as %d:%d\n", DEVNAME, MAJOR(g_dev_no), MINOR(g_dev_no));
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
MODULE_AUTHOR("textbook");
MODULE_DESCRIPTION("ring-buffer chardev demo");
