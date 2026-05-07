# Chapter 40 — Power management, runtime PM, suspend/resume

A driver that doesn't suspend cleanly is a driver that won't ship. Laptops, servers, and even VMs go through suspend/resume regularly. amdgpu's PM code is one of the gnarliest in the tree. We learn the framework now.

## 40.1 Two axes of PM

- **System suspend / resume** — user closes laptop lid. Kernel suspends every device, then the CPU. On resume, every device gets `resume()` called.
- **Runtime PM** — when a device is idle, the kernel calls `runtime_suspend`; on next access, `runtime_resume`. Per-device, while system is up.

amdgpu does both. ROCm compute systems also rely on runtime PM to reduce idle power.

## 40.2 The `dev_pm_ops` struct

```c
static const struct dev_pm_ops mydrv_pm_ops = {
    .suspend         = mydrv_suspend,        /* system suspend */
    .resume          = mydrv_resume,         /* system resume */
    .freeze          = mydrv_freeze,         /* hibernate prep */
    .thaw            = mydrv_thaw,           /* hibernate undo */
    .poweroff        = mydrv_poweroff,
    .restore         = mydrv_restore,
    SET_RUNTIME_PM_OPS(mydrv_runtime_suspend,
                       mydrv_runtime_resume,
                       mydrv_runtime_idle)
};

static struct pci_driver my_driver = {
    /* … */
    .driver.pm = &mydrv_pm_ops,
};
```

The kernel calls these at the appropriate moments.

## 40.3 What suspend does, in order

System suspend (`pm_suspend`):

1. Freeze user processes.
2. Late callbacks for each device — quiesce work.
3. Disable non-boot CPUs.
4. Suspend last "noirq" callbacks.
5. CPU suspends (S3 / S0ix on x86, off on ARM).

Resume reverses.

For your driver, in `suspend`:

- Cancel all pending work; flush workqueues.
- Wait for in-flight DMA / IRQ events to complete.
- Save register state you'll need to restore.
- Disable interrupts at the device.
- Optionally power down the PHY.

In `resume`:

- Re-enable PCIe (`pci_enable_device`, `pci_set_master`).
- Restore registers.
- Re-arm interrupts.
- Resume scheduling.

## 40.4 Runtime PM lifecycle

Reference-counted. When the count drops to 0 and the device is "idle", runtime suspend may be called.

```c
pm_runtime_get(&pdev->dev);     /* +1, may resume if needed */
/* … use device … */
pm_runtime_put(&pdev->dev);     /* -1 */
```

For sync versions: `pm_runtime_get_sync` / `pm_runtime_put_sync`.

For an autosuspend pattern (don't suspend immediately on idle):

```c
pm_runtime_set_autosuspend_delay(&pdev->dev, 5000);   /* 5s */
pm_runtime_use_autosuspend(&pdev->dev);
pm_runtime_put_autosuspend(&pdev->dev);
```

In every IO entrypoint, `pm_runtime_get_sync` to ensure the device is up. amdgpu's ioctl wrappers do this implicitly.

## 40.5 amdgpu PM landscape

The amdgpu PM code spans several systems:

- **`amdgpu_device.c::amdgpu_pmops_*`** — PCI `dev_pm_ops` callbacks.
- **`amdgpu/pm/`** — DPM (Dynamic Power Management): voltage/frequency scaling.
- **SMU** — System Management Unit: a microcontroller in the GPU running power firmware. The driver communicates via mailboxes.
- **DC** — Display Core: does its own modeset/PSR (Panel Self Refresh) for power.
- **runtime PM** — used for D3cold (full power off) when the GPU is idle and no display attached.

This is one of the most-touched areas of amdgpu in production patches. Understanding PM is a fast path to ownership of features.

## 40.6 The "atomic suspend" rule

In `*_noirq` callbacks, IRQs are disabled. You must not sleep. In `*_late` callbacks (one phase earlier), userspace is frozen but IRQs work. Pick the right phase.

amdgpu uses `pmops_prepare`, `pmops_complete`, `pmops_suspend`, `pmops_resume` plus the `_noirq` variants for the very-late ordering of GPU power.

## 40.7 D-states (PCIe)

PCIe defines D0 (active) through D3hot (deepest with config space) and D3cold (power off). The CPU can put a PCIe device in any state via config-space writes.

```c
pci_save_state(pdev);
pci_set_power_state(pdev, PCI_D3hot);
/* … later … */
pci_set_power_state(pdev, PCI_D0);
pci_restore_state(pdev);
```

For runtime suspend, amdgpu typically goes to D3hot or D3cold (the latter via ACPI hooks) depending on platform.

## 40.8 ACPI integration

ACPI provides platform-specific suspend hooks. The GPU's ACPI methods (`_PR3`, `_DSM`) are called by the kernel's PM core or directly by amdgpu. Cross-checking ACPI on a particular platform is a real debugging effort when "the lid closes but the GPU never comes back."

## 40.9 Practical advice

When you start landing patches in amdgpu PM:

- Read `Documentation/power/runtime_pm.rst`.
- Read `Documentation/driver-api/pm/devices.rst`.
- Test with `rtcwake -m mem -s 10` (RTC-triggered S3) and `echo platform | sudo tee /sys/power/disk; echo disk | sudo tee /sys/power/state` (hibernate).
- Check the platform variant: laptops use ASPM and L1.X states; desktops sometimes don't.
- Watch dmesg for `PM:` lines; they tell you what's happening at each phase.

## 40.10 Exercises

1. Add `suspend`/`resume` to your edu driver: save/restore the contents of EDU_REG_FACT.
2. Add runtime PM with autosuspend; instrument with `pm_runtime_*` calls in your read/write paths.
3. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c::amdgpu_pmops_suspend`.
4. Read `Documentation/power/runtime_pm.rst`.
5. Use `rtcwake -m mem -s 30` to trigger suspend; observe your driver's callbacks; verify state.

---

Next: [Chapter 41 — debugfs, sysfs, tracing, ftrace, perf](41-debug-and-trace.md).
