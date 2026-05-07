/*
 * edu: a PCI driver against QEMU's `edu` test device (vendor 0x1234,
 * device 0x11e8).
 *
 * Demonstrates: pci_driver, BAR mapping, MSI/INTX, IRQ handler with
 *               wait queue, character device frontend, simple register
 *               protocol.
 *
 * Lab 06 of the Linux Driver Textbook.
 *
 * Run QEMU with `-device edu` to use.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define EDU_REG_ID            0x00
#define EDU_REG_INV           0x04
#define EDU_REG_FACT          0x08      /* write input → IRQ when result ready */
#define EDU_REG_STATUS        0x20
#define EDU_REG_INTR_STATUS   0x24
#define EDU_REG_INTR_RAISE    0x60
#define EDU_REG_INTR_ACK      0x64
#define EDU_REG_DMA_SRC       0x80
#define EDU_REG_DMA_DST       0x88
#define EDU_REG_DMA_CNT       0x90
#define EDU_REG_DMA_CMD       0x98

#define EDU_DMA_FROM_DEVICE   BIT(1)
#define EDU_DMA_FIRE          BIT(0)
#define EDU_DMA_RAISE_IRQ     BIT(2)

struct edu {
    struct pci_dev *pdev;
    void __iomem   *bar0;
    int             irq;

    dev_t           dev_no;
    struct cdev     cdev;
    struct class   *cls;
    struct mutex    lock;
    wait_queue_head_t wq;
    u32             last_factorial;
    bool            irq_seen;
};

static irqreturn_t edu_isr(int irq, void *cookie)
{
    struct edu *e = cookie;
    u32 s = readl(e->bar0 + EDU_REG_INTR_STATUS);

    if (!s) return IRQ_NONE;

    writel(s, e->bar0 + EDU_REG_INTR_ACK);

    if (s & 0x1) {
        e->last_factorial = readl(e->bar0 + EDU_REG_FACT);
        WRITE_ONCE(e->irq_seen, true);
        wake_up_interruptible(&e->wq);
    }
    return IRQ_HANDLED;
}

static int edu_open(struct inode *i, struct file *f)
{
    f->private_data = container_of(i->i_cdev, struct edu, cdev);
    return 0;
}

static ssize_t edu_write(struct file *f, const char __user *ub,
                         size_t n, loff_t *off)
{
    struct edu *e = f->private_data;
    char buf[16];
    u32  v;

    if (!n || n > sizeof(buf) - 1) return -EINVAL;
    if (copy_from_user(buf, ub, n)) return -EFAULT;
    buf[n] = '\0';
    if (kstrtouint(buf, 0, &v)) return -EINVAL;

    if (mutex_lock_interruptible(&e->lock)) return -ERESTARTSYS;
    WRITE_ONCE(e->irq_seen, false);
    writel(v, e->bar0 + EDU_REG_FACT);
    mutex_unlock(&e->lock);
    return n;
}

static ssize_t edu_read(struct file *f, char __user *ub,
                        size_t n, loff_t *off)
{
    struct edu *e = f->private_data;
    char buf[32];
    int len;
    u32 val;

    if (wait_event_interruptible(e->wq, READ_ONCE(e->irq_seen)))
        return -ERESTARTSYS;

    if (mutex_lock_interruptible(&e->lock)) return -ERESTARTSYS;
    val = e->last_factorial;
    mutex_unlock(&e->lock);

    len = scnprintf(buf, sizeof(buf), "%u\n", val);
    if (*off >= len) return 0;
    if (n > (size_t)(len - *off)) n = len - *off;
    if (copy_to_user(ub, buf + *off, n)) return -EFAULT;
    *off += n;
    return n;
}

static const struct file_operations edu_fops = {
    .owner   = THIS_MODULE,
    .open    = edu_open,
    .read    = edu_read,
    .write   = edu_write,
    .llseek  = no_llseek,
};

static int edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct edu *e;
    int ret;

    e = devm_kzalloc(&pdev->dev, sizeof(*e), GFP_KERNEL);
    if (!e) return -ENOMEM;
    e->pdev = pdev;
    mutex_init(&e->lock);
    init_waitqueue_head(&e->wq);

    ret = pcim_enable_device(pdev);
    if (ret) return ret;

    ret = pcim_iomap_regions(pdev, BIT(0), "edu");
    if (ret) return ret;
    e->bar0 = pcim_iomap_table(pdev)[0];

    pci_set_master(pdev);

    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_INTX);
    if (ret < 0) return ret;
    e->irq = pci_irq_vector(pdev, 0);

    ret = devm_request_irq(&pdev->dev, e->irq, edu_isr, IRQF_SHARED, "edu", e);
    if (ret) goto err_free_irq;

    ret = alloc_chrdev_region(&e->dev_no, 0, 1, "edu");
    if (ret) goto err_free_irq;

    cdev_init(&e->cdev, &edu_fops);
    e->cdev.owner = THIS_MODULE;
    ret = cdev_add(&e->cdev, e->dev_no, 1);
    if (ret) goto err_unreg;

    e->cls = class_create("edu");
    if (IS_ERR(e->cls)) { ret = PTR_ERR(e->cls); goto err_del; }
    device_create(e->cls, &pdev->dev, e->dev_no, NULL, "edu");

    pci_set_drvdata(pdev, e);
    dev_info(&pdev->dev, "edu probed: id=0x%x irq=%d\n",
             readl(e->bar0 + EDU_REG_ID), e->irq);
    return 0;

err_del:    cdev_del(&e->cdev);
err_unreg:  unregister_chrdev_region(e->dev_no, 1);
err_free_irq:
    pci_free_irq_vectors(pdev);
    return ret;
}

static void edu_remove(struct pci_dev *pdev)
{
    struct edu *e = pci_get_drvdata(pdev);
    device_destroy(e->cls, e->dev_no);
    class_destroy(e->cls);
    cdev_del(&e->cdev);
    unregister_chrdev_region(e->dev_no, 1);
    pci_free_irq_vectors(pdev);
}

static const struct pci_device_id edu_ids[] = {
    { PCI_DEVICE(0x1234, 0x11e8) },
    { },
};
MODULE_DEVICE_TABLE(pci, edu_ids);

static struct pci_driver edu_pci_driver = {
    .name     = "edu",
    .id_table = edu_ids,
    .probe    = edu_probe,
    .remove   = edu_remove,
};

module_pci_driver(edu_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("textbook");
MODULE_DESCRIPTION("PCI driver for QEMU edu device");
