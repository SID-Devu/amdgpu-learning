# Chapter 34 — The driver model: kobject, sysfs, devices, drivers, buses

The Linux **driver model** is the kernel's universal abstraction: every device is a `struct device`, every driver is a `struct device_driver`, and they're matched on a bus. This is the spine that holds DRM, PCI, USB, and amdgpu together.

## 34.1 The objects

```
                  ┌────────────┐
                  │  kobject   │   refcounted, sysfs-visible
                  └─────┬──────┘
                        │ embedded in
        ┌───────────────┴────────────────┐
        ▼                                ▼
   ┌──────────┐                   ┌──────────┐
   │  device  │                   │  bus_type│
   └──────────┘                   └──────────┘
        │
   ┌────┴────────┐
   ▼             ▼
 device_driver  attribute_group  (sysfs files)
```

- **`struct kobject`** — the base. Has refcount, parent, name, ktype, release.
- **`struct device`** — embeds kobject. Represents a physical or logical device.
- **`struct device_driver`** — drivers register here. Has `probe`, `remove`, `id_table`.
- **`struct bus_type`** — PCI, USB, platform, I2C, SPI, etc. Match function bridges drivers and devices.

When a device appears (boot, hotplug), the bus calls each driver's `match` against the device. If matched, `probe` is called. If `probe` returns 0, the driver "binds" — the device is operational.

## 34.2 Buses you'll touch

| Bus | Used by |
|---|---|
| `pci` | PCIe devices: GPUs, NVMe, NICs |
| `platform` | SoC IP blocks (Qualcomm, Tegra, ARM SoCs) |
| `usb` | USB devices |
| `i2c` | low-speed sensors, EDID readers, GPU side-band |
| `spi` | SPI flash, sensors |
| `mdio` | PHY chips |
| `dma-buf` | (not a bus; cross-driver buffer sharing) |

amdgpu binds to PCI. AMD APUs may also have platform-bus children for sub-IPs.

## 34.3 sysfs

Every kobject creates a directory under `/sys`. The tree mirrors the device hierarchy.

```bash
ls /sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/
```

That's a typical AMD GPU PCIe path. Inside you find: `vendor`, `device`, `revision`, `power/`, `resource`, `enable`, `subsystem` (symlink to bus type), `driver` (symlink to bound driver).

Drivers expose attributes via:

```c
static ssize_t my_attr_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%d\n", value);
}

static ssize_t my_attr_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, size_t count)
{
    int v;
    if (kstrtoint(buf, 0, &v)) return -EINVAL;
    set_value(v);
    return count;
}
DEVICE_ATTR_RW(my_attr);   /* generates dev_attr_my_attr */
```

Register:

```c
device_create_file(dev, &dev_attr_my_attr);
```

Or via attribute groups:

```c
static struct attribute *my_attrs[] = {
    &dev_attr_my_attr.attr,
    NULL,
};
ATTRIBUTE_GROUPS(my);     /* makes my_groups */

driver.dev_groups = my_groups;
```

amdgpu exposes hundreds of sysfs attributes for power management, tuning, and debugging. `cat /sys/class/drm/card0/device/pp_dpm_sclk` etc.

## 34.4 The probe/remove flow

```c
static int my_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct mydrv *m = devm_kzalloc(&pdev->dev, sizeof *m, GFP_KERNEL);
    if (!m) return -ENOMEM;

    pci_set_drvdata(pdev, m);

    if (pcim_enable_device(pdev)) return -ENODEV;
    if (pcim_iomap_regions(pdev, BIT(0), "mydrv")) return -ENOMEM;
    m->mmio = pcim_iomap_table(pdev)[0];

    if (devm_request_irq(&pdev->dev, pdev->irq, my_isr, IRQF_SHARED, "mydrv", m))
        return -EIO;

    /* register chardev / drm */

    return 0;
}

static void my_remove(struct pci_dev *pdev)
{
    /* drm/chardev unregister */
    /* devm cleanup is automatic */
}

static const struct pci_device_id my_ids[] = {
    { PCI_DEVICE(0x1234, 0x5678) },
    { },
};
MODULE_DEVICE_TABLE(pci, my_ids);

static struct pci_driver my_pci_driver = {
    .name      = "mydrv",
    .id_table  = my_ids,
    .probe     = my_probe,
    .remove    = my_remove,
};

module_pci_driver(my_pci_driver);   /* expands init/exit */
```

This is the *real* shape of a PCI driver. We'll write a working version against the QEMU `edu` device in Chapter 37.

## 34.5 Hot-plug

When the bus detects a new device (e.g. PCI hot-plug, USB plug-in), the kernel calls `bus->match()` against each registered driver. If matched, `driver->probe()`. On removal, `remove`.

amdgpu doesn't typically hot-plug, but eGPU systems do. Code must work for "device disappears" — that's where `remove` runs and you must not crash on in-flight ioctls.

## 34.6 The component framework

Some subsystems need multiple devices to become operational together (e.g. a display IP block + a DDC IP block + a clock source). The **component** framework defers binding until all required components register, then calls `master_bind`. Drivers using DC for AMD displays touch this.

## 34.7 Class

A *class* groups devices with similar function for userspace. `/sys/class/drm/`, `/sys/class/gpu/`, `/sys/class/net/`. `class_create()` registers; `device_create()` adds a device.

DRM uses class to build `/sys/class/drm/cardN`. You usually don't touch `class_create` directly when writing a DRM driver — it does it for you.

## 34.8 Power-management hooks

The driver model includes PM ops (we'll cover in Chapter 40):

```c
static const struct dev_pm_ops mydrv_pm_ops = {
    .suspend = mydrv_suspend,
    .resume  = mydrv_resume,
    SET_RUNTIME_PM_OPS(mydrv_runtime_suspend, mydrv_runtime_resume, NULL)
};

static struct pci_driver my_pci_driver = {
    /* … */
    .driver.pm = &mydrv_pm_ops,
};
```

amdgpu has a complex PM ops table — runtime PM, system suspend, hibernate, ACPI integration.

## 34.9 Attaching to the device tree (briefly, for SoCs)

On embedded ARM/RISC-V platforms (Qualcomm, MediaTek, NXP), devices are described in a **device tree** (DT) blob the bootloader passes. Platform-bus drivers match DT compatible strings:

```c
static const struct of_device_id my_of_ids[] = {
    { .compatible = "vendor,my-ip" },
    { },
};
```

`amdgpu` is PCI-only; for Qualcomm-style SoC GPUs you'd be in `drivers/gpu/drm/msm/` (Adreno) which uses platform/DT.

## 34.10 Reading source

- `drivers/base/core.c` — device model core.
- `drivers/base/bus.c`, `drivers/base/driver.c` — bus matching.
- `drivers/pci/pci-driver.c` — PCI driver registration.
- `include/linux/device.h`, `include/linux/pci.h` — the headers.

Once these are familiar, every driver in the tree follows the same shape.

## 34.11 Exercises

1. Read the directory tree of one PCI device under `/sys/devices/pci0000:00/...`. Identify the symlinks `driver`, `subsystem`, `power`.
2. Write a platform driver that matches a fake compatible string (you don't need a real device — just register the driver and register a `platform_device` from another module).
3. Add a `DEVICE_ATTR_RW(foo)` to your skeleton driver from Chapter 29. Write/read it via `/sys/devices/...`.
4. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c::amdgpu_pci_driver`. Trace `probe` to identify the first ten things amdgpu does on bind.
5. `udevadm monitor --kernel` while plugging/unplugging USB; observe events. The same machinery underlies `amdgpu` reset paths.

---

End of Part V. Move to [Part VI — Linux device drivers](../part6-drivers/35-char-drivers.md).
