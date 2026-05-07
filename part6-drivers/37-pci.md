# Chapter 37 — The PCI/PCIe subsystem and writing a PCI driver

Every modern AMD GPU is a PCIe device. Mastering PCI is a hard prerequisite. This chapter walks the bus, enumeration, BARs, MSI/MSI-X, and ends with a working driver against QEMU's `edu` test device.

## 37.1 The PCI fabric

PCIe is a tree of point-to-point serial links rooted at the **Root Complex** in the CPU. Switches branch the tree; endpoints are leaves.

Each device has:

- **Bus / Device / Function** address (`BB:DD.F`): `lspci` shows them.
- **Configuration space** (256 B legacy + 4 KiB extended): vendor/device IDs, BARs, capabilities.
- **Memory BARs** — Base Address Registers: regions of physical address space mapped to the device. The OS programs the addresses; the device responds to them.
- **I/O BARs** (legacy x86 port I/O) — rarely used today.
- **Interrupts** — INTx (legacy), MSI, or MSI-X.

`lspci -tv`:

```
-[0000:00]-+-00.0  AMD Family 17h Root Complex
           +-01.0-[01]----00.0  AMD Navi 10 [Radeon RX 5700]
```

`lspci -vv -s 01:00.0` dumps everything.

## 37.2 The configuration space

A PCI config-space header looks like:

```
00: Vendor ID  Device ID
04: Command    Status
08: Class      SubClass  ProgIF  RevID
0C: …
10: BAR0
14: BAR1
18: BAR2
1C: BAR3
20: BAR4
24: BAR5
28: CardBus CIS pointer
2C: SubVendor SubDevice
30: ROM BAR
34: Cap pointer    (chain into MSI/MSI-X/PM/PCIe extended caps)
3C: IRQ            Pin  Min_Gnt  Max_Lat
```

The kernel's PCI core parses this and populates `struct pci_dev`.

## 37.3 The driver model on PCI

```c
static const struct pci_device_id mydrv_ids[] = {
    { PCI_DEVICE(0x1234, 0x11e8) },     /* QEMU edu device */
    { },
};
MODULE_DEVICE_TABLE(pci, mydrv_ids);

static int my_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void my_remove(struct pci_dev *pdev);

static struct pci_driver my_pci_driver = {
    .name     = "mydrv",
    .id_table = mydrv_ids,
    .probe    = my_probe,
    .remove   = my_remove,
};

module_pci_driver(my_pci_driver);
```

`module_pci_driver` macro declares `module_init`/`module_exit` that register/unregister the PCI driver.

## 37.4 The QEMU `edu` device

QEMU ships a pedagogic PCI device called `edu` (in `hw/misc/edu.c`). Vendor `0x1234`, device `0x11e8`. It's a minimal device with:

- a 1 MB BAR0 with control registers and a 4 KiB scratchpad,
- DMA support (memcpy from/to host RAM),
- a doorbell-style "raise IRQ" mechanism.

To attach it to a QEMU VM:

```bash
qemu-system-x86_64 -enable-kvm -m 2G -smp 2 \
    -kernel arch/x86/boot/bzImage \
    -append "root=/dev/sda console=ttyS0 nokaslr" \
    -hda rootfs.qcow2 \
    -nographic \
    -device edu
```

Inside the VM you'll see `lspci` print the edu device. We will write a real driver for it now.

## 37.5 Probe path

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

struct edu {
    struct pci_dev *pdev;
    void __iomem   *bar0;
    int             irq;

    struct cdev     cdev;
    dev_t           dev_no;
    struct class   *cls;
    spinlock_t      lock;
    wait_queue_head_t wq;
    u32             last_factorial;
    bool            irq_seen;
};

#define EDU_REG_ID         0x00
#define EDU_REG_CARD       0x04
#define EDU_REG_FACT       0x08      /* write to compute factorial */
#define EDU_REG_STATUS     0x20
#define EDU_REG_INTR_RAISE 0x60
#define EDU_REG_INTR_ACK   0x64
#define EDU_REG_INTR_STATUS 0x24

static irqreturn_t edu_isr(int irq, void *cookie)
{
    struct edu *e = cookie;
    u32 s = readl(e->bar0 + EDU_REG_INTR_STATUS);
    if (!s) return IRQ_NONE;

    writel(s, e->bar0 + EDU_REG_INTR_ACK);

    spin_lock(&e->lock);
    e->last_factorial = readl(e->bar0 + EDU_REG_FACT);
    e->irq_seen       = true;
    spin_unlock(&e->lock);

    wake_up_interruptible(&e->wq);
    return IRQ_HANDLED;
}

static ssize_t edu_write(struct file *f, const char __user *ub,
                         size_t n, loff_t *off)
{
    struct edu *e = f->private_data;
    char buf[16];
    u32  v;

    if (n == 0 || n > sizeof(buf) - 1) return -EINVAL;
    if (copy_from_user(buf, ub, n)) return -EFAULT;
    buf[n] = '\0';
    if (kstrtouint(buf, 0, &v)) return -EINVAL;

    spin_lock_irq(&e->lock);
    e->irq_seen = false;
    spin_unlock_irq(&e->lock);

    /* "factorial" register: writing N triggers IRQ when N! is computed */
    writel(v, e->bar0 + EDU_REG_FACT);
    return n;
}

static ssize_t edu_read(struct file *f, char __user *ub,
                        size_t n, loff_t *off)
{
    struct edu *e = f->private_data;
    char  buf[32];
    int   len;
    u32   val;

    if (wait_event_interruptible(e->wq, READ_ONCE(e->irq_seen)))
        return -ERESTARTSYS;

    spin_lock_irq(&e->lock);
    val = e->last_factorial;
    spin_unlock_irq(&e->lock);

    len = scnprintf(buf, sizeof(buf), "%u\n", val);
    if (*off >= len) return 0;
    if (n > len - *off) n = len - *off;
    if (copy_to_user(ub, buf + *off, n)) return -EFAULT;
    *off += n;
    return n;
}

static int edu_open(struct inode *i, struct file *f)
{
    struct edu *e = container_of(i->i_cdev, struct edu, cdev);
    f->private_data = e;
    return 0;
}

static const struct file_operations edu_fops = {
    .owner  = THIS_MODULE,
    .open   = edu_open,
    .read   = edu_read,
    .write  = edu_write,
    .llseek = no_llseek,
};

static int edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct edu *e;
    int ret;

    e = devm_kzalloc(&pdev->dev, sizeof(*e), GFP_KERNEL);
    if (!e) return -ENOMEM;
    e->pdev = pdev;
    spin_lock_init(&e->lock);
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
    dev_info(&pdev->dev, "edu probed: bar0=%p irq=%d id=0x%x\n",
             e->bar0, e->irq, readl(e->bar0 + EDU_REG_ID));
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
```

Userspace test:

```bash
sudo insmod edu.ko
echo 5 | sudo tee /dev/edu     # 5! = 120
sudo cat /dev/edu              # prints 120
```

You wrote a complete PCI driver: enumerated, ioremap'd a BAR, requested an IRQ, built a chardev front-end, and validated with hardware (well, virtual hardware).

## 37.6 What's important

- `pcim_*` (managed) variants auto-clean on detach. Prefer them.
- Always `pci_set_master` before doing DMA.
- `pci_alloc_irq_vectors` — modern API; tries MSI-X, MSI, INTx in priority.
- `readl/writel` on `__iomem` memory; never deref directly.
- Use `dev_*` for prints — they include the BDF.

## 37.7 Beyond `edu`

Real GPUs have:

- many BARs (frame buffer, MMIO registers, doorbells),
- 64-bit BARs (`PCI_BASE_ADDRESS_MEM_TYPE_64`),
- multiple MSI-X vectors per IRQ source,
- PCIe link training / state,
- ASPM (Active State Power Management),
- Resizable BAR (so the CPU can map all of VRAM if the system supports it).

The amdgpu probe path enumerates all of these; you'll read it in Chapter 48.

## 37.8 Exercises

1. Build the edu driver against your kernel; run in QEMU; verify factorial.
2. Add an ioctl to read `EDU_REG_ID` (a fixed value 0xed).
3. Use the edu's DMA registers (read QEMU's `hw/misc/edu.c`) — kick a host→device→host memcpy.
4. Read `Documentation/PCI/pci.rst`. Long but useful.
5. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c::amdgpu_pci_probe`. Note how it differs from edu in scope; identify the analog of `pcim_iomap_regions`, `pci_alloc_irq_vectors`, `pci_set_drvdata`.

---

Next: [Chapter 38 — DMA, IOMMU, coherent vs streaming mappings](38-dma-and-iommu.md).
