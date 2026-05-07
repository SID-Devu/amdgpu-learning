# Lab 06 — PCI driver against QEMU's `edu` device

Real PCI driver: BARs, IRQ, chardev front-end. Test in a QEMU VM.

## Test setup (QEMU)

```bash
qemu-system-x86_64 -enable-kvm -m 2G -smp 2 \
    -kernel /boot/vmlinuz-$(uname -r) \
    -append "root=/dev/sda console=ttyS0 nokaslr" \
    -drive file=ubuntu.qcow2,if=virtio \
    -nographic \
    -device edu
```

Inside the VM:

```bash
lspci -nn | grep edu     # 1234:11e8
make load
echo 5 > /dev/edu        # 5! = 120
cat  /dev/edu            # prints 120
make unload
```

`dmesg | tail` shows probe/remove messages.

## Stretch

1. Add DMA support: allocate a coherent buffer; fill with a pattern; DMA into the device's internal scratchpad; DMA back; verify.
2. Add an `ioctl` to read the `EDU_REG_ID`.
3. Add `mmap` to expose a slice of the BAR to userspace (root only).
4. Add suspend/resume (`dev_pm_ops`) saving/restoring last factorial.
