# Chapter 22 — Interrupts, exceptions, IRQ delivery

A driver without interrupts is a polling driver — slow, CPU-hogging, fragile. Real hardware *interrupts the CPU* when something happens (DMA done, frame finished, error). This chapter covers the interrupt architecture from silicon up to your `irq_handler_t`.

## 22.1 What an interrupt is

An interrupt is the CPU saying: "stop what you're doing, jump to a handler, then resume." There are three flavors:

1. **Synchronous exceptions / faults** — caused by execution: page fault, division by zero, GPF.
2. **Hardware interrupts** — async, from a device.
3. **Software interrupts (IPIs)** — from another CPU.

We focus on (2). On x86_64:

- Each interrupt has a vector 0–255.
- A table (the **IDT**) maps vector → handler.
- The CPU pushes RIP/CS/RFLAGS, switches to the kernel stack (via IST or normal), jumps.

## 22.2 Legacy and modern interrupt delivery

Three eras of interrupt routing on PCs:

1. **PIC / 8259** — original, 16 IRQ lines. Obsolete except for emulated.
2. **APIC / IOAPIC** — per-CPU local APIC + chipset I/O APIC. Supports many vectors and per-CPU delivery.
3. **MSI / MSI-X** — Message-Signaled Interrupts. The device, instead of toggling a wire, **writes** a value to a special memory address that triggers an interrupt. The address encodes "which CPU + which vector"; the data is the vector.

PCIe devices today use MSI/MSI-X. amdgpu requests MSI-X with multiple vectors so different GPU rings can deliver to different CPUs.

```c
ret = pci_alloc_irq_vectors(pdev, 1, max, PCI_IRQ_MSIX | PCI_IRQ_MSI);
```

This is the modern API.

## 22.3 The kernel's IRQ subsystem

Each device IRQ has a `struct irq_desc` with:

- a chip (the controller it belongs to: `apic_chip`, `intel_ir_chip`, `amd_iommu_chip`),
- a list of action handlers,
- statistics, affinity, etc.

You request an IRQ:

```c
ret = request_irq(irq, my_isr, IRQF_SHARED, "mydrv", dev);
```

On free:

```c
free_irq(irq, dev);
```

The cookie (`dev`) is passed to your handler so you can find your driver's state.

## 22.4 The handler

```c
static irqreturn_t my_isr(int irq, void *cookie)
{
    struct mydrv *m = cookie;
    u32 status;

    status = readl(m->regs + REG_INT_STATUS);
    if (!(status & INT_BITS_OF_INTEREST))
        return IRQ_NONE;          /* not ours */

    writel(status, m->regs + REG_INT_ACK);

    /* schedule deferred work; do little here */
    schedule_work(&m->work);
    return IRQ_HANDLED;
}
```

Rules of an ISR (top half):

- **Cannot sleep**, ever.
- **Must be fast** (microseconds).
- Should detect "is this even our interrupt?" first if shared.
- Acknowledge the device.
- Defer real work to a softirq, tasklet, workqueue, or threaded IRQ.

Returning `IRQ_NONE` from a non-shared IRQ is a bug; from a shared one, it's normal when the IRQ wasn't ours.

## 22.5 Top half and bottom half

The classic kernel pattern: short top half (hard IRQ context) ACKs and queues; bottom half does the heavy lifting in a context that can take locks/sleep.

Bottom-half mechanisms:

- **Softirq** — built-in, fixed slots (NET_RX, BLOCK, etc.). Drivers don't add new ones.
- **Tasklet** — softirq-based, easy API. Atomic context. Deprecated in newer kernels in favor of workqueues.
- **Workqueue** — runs in a kthread, can sleep. Default for drivers.
- **Threaded IRQ** — request_threaded_irq with primary handler + thread function.

For most drivers today, **threaded IRQ + workqueue** is the right answer.

```c
ret = request_threaded_irq(irq,
                           my_isr_quick,    /* hardirq */
                           my_isr_thread,   /* threaded */
                           IRQF_SHARED, "mydrv", dev);
```

`my_isr_quick` runs in hardirq, ACKs the device, returns `IRQ_WAKE_THREAD`. `my_isr_thread` runs in a kthread, can sleep.

## 22.6 amdgpu interrupt flow

amdgpu has many sources: vertical-blank, command-submission completion, SDMA, RAS errors, page-fault, IH (Interrupt Handler) ring.

The hardware writes interrupt cookies into the **IH ring** — a circular buffer in system memory. amdgpu's hardirq handler looks at the IH ring head/tail, copies entries, schedules a tasklet/work to process them. This avoids needing one IRQ per event.

You'll read this in `drivers/gpu/drm/amd/amdgpu/amdgpu_irq.c` once we get to Part VII.

## 22.7 Affinity

`/proc/irq/<n>/smp_affinity` lets the user pin an IRQ to a CPU set. For RT systems and high-throughput servers, irq affinity is critical. NVMe pins each MSI-X to a different CPU.

Driver code can hint via `pci_irq_get_affinity()`.

## 22.8 NMI and watchdogs

The Non-Maskable Interrupt is reserved for things that can't be ignored: hardware errors, watchdog timeouts. The kernel's hardlockup detector uses an NMI to detect when a CPU is stuck for too long. As a driver author you'll see NMI traces in the dmesg when the kernel panics on lockup.

## 22.9 IPIs

Inter-Processor Interrupts: a CPU sends an interrupt to another CPU. Used for TLB shootdown, kicking idle CPUs, RCU. Drivers rarely use IPIs directly — `smp_call_function*` wraps it.

## 22.10 Receiving interrupts in atomic vs. process context

One of the most common bugs: holding a regular spinlock when an interrupt fires that *also* takes the spinlock. Deadlock.

Fix: use **`spin_lock_irqsave`** when the same lock might be taken from IRQ context. Disables IRQs while held. Pair with `spin_unlock_irqrestore`. Not free; only when needed.

We'll spend a chapter on locks in Part V.

## 22.11 Polling fallback

When a chip's interrupt is broken (firmware bug, unsupported chip), drivers fall back to polling. amdgpu has knobs to disable certain interrupts and poll instead. Useful for triage.

## 22.12 Exercises

1. `cat /proc/interrupts` — note the columns: per-CPU counts. Observe which IRQs land on which cores. Find your GPU's IRQ.
2. Read `Documentation/core-api/genericirq.rst`.
3. Read `kernel/irq/manage.c::request_threaded_irq`.
4. In a kernel module, request an IRQ for a device you actually have (e.g. a serial port at IRQ 4 on QEMU). Print "irq fired" with `pr_info`. Free it on unload.
5. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_irq.c::amdgpu_irq_handler` once. Just orient yourself — we'll come back.

---

End of Part III. Move to [Part IV — Multithreading & concurrency](../part4-multithreading/23-threads-and-memory-model.md).
