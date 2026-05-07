# Chapter 9 — Function pointers, callbacks, opaque types, plugin design

The Linux kernel is "C with virtual functions." It implements polymorphism using **structs of function pointers**. Once you internalize this, the kernel suddenly becomes legible.

## 9.1 The syntax, one more time

```c
int add(int a, int b) { return a + b; }

int (*fp)(int, int) = add;     /* pointer to function */
int (*fp2)(int, int) = &add;   /* same — & is optional */

int r = fp(2, 3);              /* call through pointer */
int r2 = (*fp)(2, 3);          /* same */
```

Use `typedef` for readability:

```c
typedef int (*binop_fn)(int, int);
binop_fn ops[] = { add, sub, mul, div };
```

To return a function pointer, the syntax is dreadful — let `typedef` save you:

```c
typedef int (*op_t)(int, int);
op_t pick(char c) { return c == '+' ? add : sub; }
```

## 9.2 Callbacks

Pass behavior as data:

```c
void each(int *a, size_t n, void (*fn)(int *)) {
    for (size_t i = 0; i < n; i++) fn(&a[i]);
}

static void doubl(int *p) { *p *= 2; }

each(arr, 100, doubl);
```

Always pass a context pointer alongside:

```c
void each_ctx(int *a, size_t n, void (*fn)(int *, void *), void *ctx);
```

Callbacks without context become global state and won't compose. The kernel's qsort (`sort()`) takes a `cmp` callback but no ctx; you have to use a per-CPU variable or container_of() trick. Don't repeat that mistake in your own APIs.

## 9.3 Vtables in C: structs of function pointers

```c
struct file_operations {
    struct module *owner;
    int     (*open)(struct inode *, struct file *);
    int     (*release)(struct inode *, struct file *);
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    long    (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    int     (*mmap)(struct file *, struct vm_area_struct *);
    /* … */
};
```

Each driver instantiates one of these as a `static const` and registers it. The VFS calls `f->f_op->read(...)` polymorphically.

```c
static const struct file_operations my_fops = {
    .owner  = THIS_MODULE,
    .open   = my_open,
    .release= my_release,
    .read   = my_read,
    .write  = my_write,
    .unlocked_ioctl = my_ioctl,
};
```

This is the **single most important pattern** in kernel C. Every subsystem looks like this:

- `struct pci_driver` — for PCI device drivers
- `struct platform_driver` — for SoC devices
- `struct drm_driver` — for DRM drivers
- `struct dma_fence_ops` — for fence operations
- `struct ttm_device_funcs` — for TTM
- `struct drm_sched_backend_ops` — for the GPU scheduler

Once you know "find the ops struct" you can navigate any subsystem.

## 9.4 Opaque types and the C version of OOP

Encapsulation without classes:

```c
/* counter.h */
struct counter;
struct counter *counter_create(void);
void  counter_destroy(struct counter *);
void  counter_inc(struct counter *);
int   counter_value(const struct counter *);
```

```c
/* counter.c */
struct counter { int n; };
struct counter *counter_create(void) {
    struct counter *c = malloc(sizeof *c);
    if (c) c->n = 0;
    return c;
}
void counter_destroy(struct counter *c) { free(c); }
void counter_inc(struct counter *c) { c->n++; }
int  counter_value(const struct counter *c) { return c->n; }
```

The kernel uses opaque types extensively: `struct mutex`, `struct dma_fence`, `struct drm_device` — you hold pointers, you don't peek inside.

## 9.5 Inheritance via embedded structs

```c
struct base {
    int x;
};

struct derived {
    struct base parent;   /* must be first or use container_of */
    int y;
};

void use_base(struct base *b);

struct derived d = { .parent.x = 1, .y = 2 };
use_base(&d.parent);            /* upcast */
struct derived *dp = container_of(b, struct derived, parent);  /* downcast */
```

This is exactly how `struct kobject` and `struct device` are used. `kobject` is the "base class" of every object that appears in `/sys`; embed it, and your object is sysfs-visible.

```c
struct my_dev {
    struct device dev;     /* embed */
    void __iomem *mmio;
    int           irq;
    /* … */
};
```

## 9.6 The `_init`/`_destroy` symmetry

Every constructor needs a destructor. Driver code is heavy with paired init/destroy; if you forget destroy you leak, often silently for hours.

```c
struct foo {
    void   *buf;
    size_t  cap;
};

int  foo_init(struct foo *f, size_t cap)
{
    f->buf = malloc(cap);
    if (!f->buf) return -ENOMEM;
    f->cap = cap;
    return 0;
}

void foo_fini(struct foo *f)
{
    free(f->buf);
    f->buf = NULL;
    f->cap = 0;
}
```

For probe/remove paths, every step adds an "undo" we must perform on error or remove. Use **goto chains**:

```c
int probe(struct device *dev)
{
    struct mydrv *m = kzalloc(sizeof *m, GFP_KERNEL);
    if (!m)                     return -ENOMEM;

    m->mmio = ioremap(addr, size);
    if (!m->mmio)               { ret = -ENOMEM; goto err_free; }

    ret = request_irq(irq, isr, 0, "mydrv", m);
    if (ret)                    goto err_unmap;

    ret = register_chrdev(...);
    if (ret)                    goto err_free_irq;

    return 0;

err_free_irq:   free_irq(irq, m);
err_unmap:      iounmap(m->mmio);
err_free:       kfree(m);
    return ret;
}
```

Goto for cleanup chains is **idiomatic** in the kernel and *encouraged* by the kernel coding style. Don't be afraid of it.

## 9.7 Plugin design

A "plugin" registers itself with a core via a function pointer table. The core walks a list of registered plugins and calls into them.

```c
/* core.h */
struct codec_ops {
    const char *name;
    int (*encode)(const uint8_t *in, size_t n, uint8_t *out);
};

int  codec_register(const struct codec_ops *ops);
void codec_unregister(const struct codec_ops *ops);
const struct codec_ops *codec_find(const char *name);
```

```c
/* mp3.c */
static const struct codec_ops mp3_ops = {
    .name   = "mp3",
    .encode = mp3_encode,
};

static int __init mp3_init(void) { return codec_register(&mp3_ops); }
static void __exit mp3_exit(void) { codec_unregister(&mp3_ops); }
```

The Linux kernel implements every subsystem this way. `pci_register_driver`, `platform_driver_register`, `drm_dev_register` are all variations.

## 9.8 The `EXPORT_SYMBOL` mechanism

Modules can call into the kernel only via *exported* symbols:

```c
int  amdgpu_bo_create(...);
EXPORT_SYMBOL(amdgpu_bo_create);

/* or for GPL-only modules */
EXPORT_SYMBOL_GPL(amdgpu_bo_create);
```

`EXPORT_SYMBOL_GPL` restricts use to GPL-licensed modules. `lsmod` and `modinfo` show dependencies. When your module references a symbol, the linker (`modpost`) records the dep.

## 9.9 Exercises

1. Implement an in-memory key/value store with vtable for storage backends:
   ```c
   struct kv_ops {
       int (*put)(void *self, const char *k, const char *v);
       int (*get)(void *self, const char *k, char *v, size_t cap);
       int (*del)(void *self, const char *k);
   };
   ```
   Provide two backends: in-memory hashtable and a file-backed one.
2. Implement a tiny event system: `event_subscribe(name, fn, ctx)`, `event_publish(name, data)`. Multiple subscribers per event.
3. Implement a state machine using a table of `state_handler` function pointers.
4. Read `include/linux/fs.h` and find `struct file_operations`. Identify each callback. Read at least one real implementation (e.g. `drivers/char/mem.c` — `mem_fops`).
5. Read `include/drm/drm_drv.h` and find `struct drm_driver`. Skim. We will use it heavily later.

---

Next: [Chapter 10 — Defensive C: const correctness, restrict, MISRA-ish style](10-defensive-c.md).
