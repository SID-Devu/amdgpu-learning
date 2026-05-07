# Chapter 5 — Structs, unions, bitfields, packing, alignment

Structs are how C describes data. In a driver, a struct often **maps directly onto hardware registers**, a packet on the wire, or a shared memory layout between kernel and userspace. The compiler may add padding, reorder bitfields, choose an endianness — and getting any of those wrong produces silent, terrifying bugs.

## 5.1 The basic struct

```c
struct point {
    int x;
    int y;
};

struct point p = { .x = 1, .y = 2 };  /* designated initializer (C99) */
```

`sizeof(struct point) == 8` on most platforms. Members are stored in the order written. The compiler may insert **padding** to satisfy alignment.

## 5.2 Alignment

Every type has an *alignment* — the multiple of its address that it must live on. Common alignments on x86_64:

| Type | Size | Alignment |
|---|---|---|
| `char` | 1 | 1 |
| `short` | 2 | 2 |
| `int` | 4 | 4 |
| `long`, `void *` | 8 | 8 |
| `double` | 8 | 8 |

A struct's alignment is the *max* of its members'. Members are placed at the smallest offset ≥ their alignment.

```c
struct gap {
    char  a;     /* offset 0 */
    /* 3 bytes pad */
    int   b;     /* offset 4 */
    char  c;     /* offset 8 */
    /* 3 bytes pad to make sizeof a multiple of 4 */
};                /* sizeof = 12 */
```

If you reorder:

```c
struct nogap {
    int  b;      /* 0 */
    char a;      /* 4 */
    char c;      /* 5 */
    /* 2 bytes tail pad */
};               /* sizeof = 8 */
```

Order from largest to smallest to minimize padding. In hot data structures (per-CPU vars, packet headers) this affects cache footprint.

Print sizes of any struct with:

```c
printf("%zu\n", sizeof(struct gap));
```

Or use `pahole` (from `dwarves` package) to dump padding from DWARF:

```bash
pahole my_program | less
```

This is a tool you will use often when reading kernel structs.

## 5.3 Why misaligned access matters

On x86, misaligned loads usually work but are slower. On many ARM cores, on RISC-V, on older SPARC, a misaligned load *traps* (`SIGBUS` in userspace, an oops in the kernel). Driver code runs on every architecture Linux supports — so misaligned access is forbidden.

```c
char buf[8];
*(uint32_t *)(buf + 1) = 0;  /* BAD: address may be unaligned */
```

Use `memcpy`:

```c
uint32_t v = 0;
memcpy(buf + 1, &v, 4);
```

The compiler frequently generates the same single instruction on x86 and a safe sequence on ARM. This is the right idiom.

## 5.4 The `__attribute__((packed))` directive

For hardware register banks, on-the-wire packets, or anything that *must* match an external layout exactly, use `packed`:

```c
struct __attribute__((packed)) ethhdr {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;     /* big-endian on the wire */
};                          /* sizeof == 14, no padding */
```

A packed struct has alignment 1 and may produce slow access on strict-alignment CPUs. The kernel macro `__packed` is just `__attribute__((packed))`.

**Rule**: use packed only when you must match an external format. Don't pack everything.

## 5.5 Unions

A `union` overlays multiple types in the same storage. `sizeof(union)` is the size of the largest member. Reading a member that wasn't last written is implementation-defined / UB depending on the rule.

```c
union ipv4_or_bytes {
    uint32_t addr;
    uint8_t  octets[4];
};

union ipv4_or_bytes u;
u.addr = 0x0100007F;            /* 127.0.0.1 in little-endian */
printf("%u\n", u.octets[0]);    /* 127 on x86 (LE) */
```

This is the classic endianness probe. It's also used to overlay register layouts: a 32-bit register that has different bit fields depending on mode.

The kernel uses unions extensively in `struct sk_buff`, `struct page`, and many DRM IOCTL structs.

## 5.6 Bitfields

Bitfields let you name individual bits inside an integer:

```c
struct ctrl_reg {
    uint32_t enable      : 1;
    uint32_t mode        : 3;
    uint32_t _reserved   : 4;
    uint32_t length      : 16;
    uint32_t crc         : 8;
};
```

This compiles to integer mask/shift code. **But** the order of bits within the underlying word and the size of "underlying word" are implementation-defined. So bitfields are *not* portable for hardware register layouts on multi-arch code.

The portable kernel idiom for hardware registers is explicit shifts and masks:

```c
#define CTRL_ENABLE     BIT(0)
#define CTRL_MODE_MASK  GENMASK(3, 1)
#define CTRL_MODE_SHIFT 1
#define CTRL_LEN_MASK   GENMASK(23, 8)
#define CTRL_LEN_SHIFT  8

u32 v = readl(dev->ctrl);
u32 mode = (v & CTRL_MODE_MASK) >> CTRL_MODE_SHIFT;
v &= ~CTRL_LEN_MASK;
v |= (length << CTRL_LEN_SHIFT) & CTRL_LEN_MASK;
writel(v, dev->ctrl);
```

Memorize this pattern. Bitfields are nice for tightly-controlled IPC where the compiler is fixed, but for hardware registers, mask/shift is the rule.

## 5.7 Endianness

A 32-bit integer `0x12345678` lives in memory as four bytes. Two orderings exist:

- **Little-endian** (x86, ARM in default mode): bytes `78 56 34 12`. Least significant byte at lowest address.
- **Big-endian** (most network protocols, some PowerPC, MIPS): bytes `12 34 56 78`.

The kernel provides typed endian conversions:

```c
__le32 v_on_disk;             /* little-endian */
__be32 v_on_wire;             /* big-endian */
u32 v = le32_to_cpu(v_on_disk);     /* to CPU native */
__le32 d = cpu_to_le32(v);          /* to disk format */
```

Sparse (a kernel static analyzer) flags type mismatches: assigning `__be32` to `__le32` is an error. **Use these typedefs** when describing on-wire/on-disk/over-PCIe formats. They prevent endian bugs from compiling.

GPU registers on AMD are little-endian; PCIe config space is little-endian; many network protocols are big-endian. You must keep track.

## 5.8 The `container_of` macro

This is the single most important macro in the Linux kernel. Given a pointer to a *member* of a struct, it returns a pointer to the *containing* struct:

```c
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                  \
    const typeof(((type *)0)->member) *__mptr = (ptr);      \
    (type *)((char *)__mptr - offsetof(type, member)); })
```

Why? Because the kernel embeds intrusive list/tree nodes inside structs:

```c
struct task {
    int id;
    struct list_head node;   /* embedded list node */
    char name[32];
};

struct list_head *p = ... ;  /* I have a node pointer */
struct task *t = container_of(p, struct task, node);  /* recover the task */
```

This is how `list_for_each_entry`, `rb_entry`, and dozens of macros work. **Drill this until it's instinctive.**

## 5.9 Forward declarations and opaque types

You can declare a struct without defining it. This is *opaque*:

```c
/* in foo.h */
struct foo;                       /* opaque */
struct foo *foo_create(void);
void        foo_destroy(struct foo *);
int         foo_do_thing(struct foo *, int);

/* in foo.c */
struct foo {
    int internal_state;
    /* … */
};
```

Users of the header can hold a `struct foo *` but cannot peek inside. This is **C's encapsulation mechanism**. The kernel uses it heavily: a userspace driver writer holding a `struct drm_device *` cannot see its members; they go through API functions.

## 5.10 Anonymous structs and unions

C11 (and a GCC extension before that) allows nested anonymous types:

```c
struct frame {
    int seq;
    union {
        struct { uint8_t a, b; };  /* anonymous struct */
        uint16_t both;
    };
};

struct frame f = { .seq = 0, .a = 1, .b = 2 };
```

You access `.a`, `.b`, `.both` directly without an extra qualifier. The kernel uses anonymous unions in `struct page` and `struct dma_fence_cb`.

## 5.11 Real-world: a hardware-register struct

A simplified UART:

```c
struct __packed uart_regs {
    u32 data;       /* 0x00: data register */
    u32 status;     /* 0x04: status */
#define UART_STATUS_TX_EMPTY   BIT(0)
#define UART_STATUS_RX_FULL    BIT(1)
#define UART_STATUS_FRAME_ERR  BIT(2)
    u32 ctrl;       /* 0x08: control */
#define UART_CTRL_ENABLE       BIT(0)
#define UART_CTRL_BAUD_MASK    GENMASK(15, 8)
    u32 _reserved[1];
    u32 fifo_level; /* 0x10: fifo occupancy */
};

struct uart_dev {
    struct uart_regs __iomem *regs;
    /* … */
};

static int uart_putc(struct uart_dev *u, char c)
{
    while (!(readl(&u->regs->status) & UART_STATUS_TX_EMPTY))
        cpu_relax();
    writel(c, &u->regs->data);
    return 0;
}
```

Note:

- `__iomem` annotates that this pointer is into device memory, not normal RAM. Sparse will warn if you dereference it directly.
- `readl` / `writel` are 32-bit I/O accessors with appropriate barriers.
- The struct *is* the register layout. Any padding/reordering would point at the wrong register.

## 5.12 Exercises

1. Reorder this struct to minimize size:
   ```c
   struct s { char a; double b; char c; int d; char e; };
   ```
2. Write a `struct ipv4_packet` (packed) matching the IP header fields. Add the bit shifts for `version` and `IHL` since they share a byte.
3. Write `container_of` from memory and use it: define `struct task` containing an `int id;` and a `struct list_head node;`, take a `node *`, recover the `task *`, print the id.
4. Use `pahole` (or, if unavailable, hand-compute) on `struct task_struct` from your installed kernel headers; identify the largest source of padding.
5. Write a 16-bit and 32-bit `swap_bytes` function and use it instead of compiler builtins. Then read `<linux/swab.h>` to see how the kernel does it.

---

Next: [Chapter 6 — Bit manipulation: the language of hardware](06-bit-manipulation.md).
