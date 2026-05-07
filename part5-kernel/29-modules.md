# Chapter 29 — Loadable kernel modules: your first module

A module is a relocatable object (`.ko`) the kernel loads at runtime. We extend the hello-world from Chapter 28 to teach the full module API: parameters, init/exit ordering, dependencies, exported symbols.

## 29.1 Anatomy of a module

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static int  burst = 1;
static char *name = "world";

module_param(burst, int, 0644);
MODULE_PARM_DESC(burst, "how many greetings");
module_param(name, charp, 0644);
MODULE_PARM_DESC(name, "who to greet");

static int __init mod_init(void)
{
    int i;
    for (i = 0; i < burst; i++)
        pr_info("hello, %s (%d)\n", name, i);
    return 0;
}

static void __exit mod_exit(void)
{
    pr_info("goodbye, %s\n", name);
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("you");
MODULE_DESCRIPTION("greeter");
MODULE_VERSION("0.1");
```

Build with the Makefile from §28.6.

```bash
sudo insmod hello.ko name=AMD burst=3
dmesg | tail
sudo rmmod hello
```

You can also pass params via modprobe.conf or the kernel command line.

## 29.2 `__init` and `__exit` annotations

`__init` and `__exit` are *section attributes*. They place the function in special sections (`.init.text`, `.exit.text`). After init runs, the kernel **frees** the `.init.text` memory. So:

- `__init` functions cannot be called after init.
- `__init` data is free'd too.

For modules, the rules are similar but `__exit` may be elided if the module is built-in (kernel can't unload built-in modules).

`__initdata` for data, `__exitdata` for exit-only data.

## 29.3 The `printk` levels

```c
pr_emerg(...), pr_alert(...), pr_crit(...), pr_err(...),
pr_warn(...), pr_notice(...), pr_info(...), pr_debug(...)
```

`pr_debug` is compiled out unless `DEBUG` is defined or `CONFIG_DYNAMIC_DEBUG` is on. `dmesg` shows everything; userspace klogd may filter to syslog.

In drivers, `dev_*` versions tag with the device:

```c
dev_info(dev, "frobbing %d\n", n);
```

## 29.4 Module dependencies

If your module calls a function exported by another module, modprobe loads the dependency first. The build records dependencies via `EXPORT_SYMBOL` of called functions.

```c
EXPORT_SYMBOL(my_helper);
EXPORT_SYMBOL_GPL(my_gpl_helper);   /* GPL-only */
```

`modinfo hello.ko | grep depends` shows the result.

## 29.5 Symbol naming and collisions

All exported symbols share a global namespace. Conflicts result in module-load failure. Prefix everything with your module name (e.g. `amdgpu_*`). Use `static` for internal helpers.

## 29.6 Module parameters

```c
module_param_array(name, type, &count, perm);
module_param_named(public_name, internal_var, type, perm);
module_param_string(name, buf, sizeof(buf), perm);
```

Permissions: 0 = no sysfs file; 0644 = world-readable, root-writable. Read in `/sys/module/<name>/parameters/<param>`. Writeable params can be changed at runtime.

amdgpu has many params (`amdgpu_dpm`, `amdgpu_pcie_gen2`, `amdgpu_runpm`). Read `drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c` once.

## 29.7 Auto-loading via `MODULE_DEVICE_TABLE`

Drivers tell the kernel which devices they bind to via a table:

```c
static const struct pci_device_id mydrv_ids[] = {
    { PCI_DEVICE(0x1002, 0x73a3) },   /* example AMD device */
    { },
};
MODULE_DEVICE_TABLE(pci, mydrv_ids);
```

The build system extracts these into `modules.alias`. udev matches devices to aliases on hotplug and runs `modprobe`.

## 29.8 Init order

The kernel's `start_kernel` runs `do_initcalls` in order:

```
core_initcall      — early
postcore_initcall
arch_initcall
subsys_initcall
fs_initcall
device_initcall    — module_init defaults here
late_initcall
```

For drivers, `module_init` becomes `device_initcall`. If you need earlier (e.g. you provide a clock or bus), use `subsys_initcall(my_init)`.

## 29.9 The `THIS_MODULE` and refcount

```c
.owner = THIS_MODULE,
```

Each `struct file_operations` should have an owner so that modules with open file descriptors aren't unloaded out from under users. `try_module_get`/`module_put` increments/decrements the count.

## 29.10 Loading and unloading machinery

Userspace tools:

- `insmod` — load a `.ko` file directly.
- `modprobe` — resolve dependencies and load.
- `rmmod` — unload.
- `lsmod` — list.
- `modinfo` — print metadata.

Module loading goes through `init_module(2)` and `finit_module(2)` syscalls. The `finit_module` variant takes a file descriptor instead of a buffer (preferred since 3.8).

## 29.11 A non-trivial module: chardev skeleton

We won't fully implement a char device until Chapter 35, but here's the bone:

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVNAME "skel"

static dev_t           dev_no;
static struct cdev     cdev;
static struct class   *cls;

static int     skel_open(struct inode *i, struct file *f)  { return 0; }
static int     skel_release(struct inode *i, struct file *f){ return 0; }
static ssize_t skel_read(struct file *f, char __user *u, size_t n, loff_t *o) {
    return 0;
}
static ssize_t skel_write(struct file *f, const char __user *u, size_t n, loff_t *o) {
    return n;
}

static const struct file_operations skel_fops = {
    .owner   = THIS_MODULE,
    .open    = skel_open,
    .release = skel_release,
    .read    = skel_read,
    .write   = skel_write,
};

static int __init skel_init(void)
{
    int ret = alloc_chrdev_region(&dev_no, 0, 1, DEVNAME);
    if (ret) return ret;

    cdev_init(&cdev, &skel_fops);
    ret = cdev_add(&cdev, dev_no, 1);
    if (ret) goto unreg;

    cls = class_create(DEVNAME);
    if (IS_ERR(cls)) { ret = PTR_ERR(cls); goto del; }
    device_create(cls, NULL, dev_no, NULL, DEVNAME);
    return 0;

del:    cdev_del(&cdev);
unreg:  unregister_chrdev_region(dev_no, 1);
    return ret;
}

static void __exit skel_exit(void)
{
    device_destroy(cls, dev_no);
    class_destroy(cls);
    cdev_del(&cdev);
    unregister_chrdev_region(dev_no, 1);
}

module_init(skel_init);
module_exit(skel_exit);
MODULE_LICENSE("GPL");
```

Insmod, then `ls -l /dev/skel`. You created a real device file. Read returns 0; write returns n. We'll fill in the body in Chapter 35.

## 29.12 Exercises

1. Write the param-greeter from §29.1; run with multiple param values; observe.
2. Build the chardev skeleton; verify `/dev/skel` appears; `cat /dev/skel` returns nothing; `echo hi > /dev/skel` accepts the bytes.
3. Add an `EXPORT_SYMBOL(my_func)` and a second module that calls it; observe the dependency in `modinfo`.
4. Examine `lsmod | grep amdgpu`; identify dependencies (`drm_kms_helper`, `drm`, `iommu_v2`, etc.).
5. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c::amdgpu_init`. Recognize `module_param`, `module_init` patterns.

---

Next: [Chapter 30 — Kernel data structures: lists, rbtrees, xarray, idr](30-kernel-data-structures.md).
