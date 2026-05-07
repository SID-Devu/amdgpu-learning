# Appendix — A QEMU + custom-kernel dev VM (do this once, save your life forever)

You will break the kernel. You don't want to brick your laptop. The right answer is a VM that boots a kernel *you* built, with `gdb` attached, and that you can blow away in 5 seconds.

## 1. Install host packages

Debian/Ubuntu:

```bash
sudo apt install -y \
    build-essential libncurses-dev libssl-dev libelf-dev bison flex \
    bc cpio rsync wget git \
    qemu-system-x86 qemu-utils \
    debootstrap
```

## 2. Get the kernel source

```bash
cd ~/work
git clone --depth=1 https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
cd linux
```

## 3. Configure and build

Start from the Debian/Ubuntu config (closest to a real distro), enable debug:

```bash
make defconfig
make kvm_guest.config             # adds virtio etc.
scripts/config --enable DEBUG_KERNEL --enable DEBUG_INFO_DWARF5 \
               --enable GDB_SCRIPTS --enable PROVE_LOCKING \
               --enable KASAN --enable KCSAN \
               --enable DEBUG_FS --enable FTRACE --enable DYNAMIC_FTRACE
make olddefconfig
make -j$(nproc)
```

You now have `arch/x86/boot/bzImage` and `vmlinux`.

## 4. Build a minimal rootfs

The classic "buildroot or debootstrap" choice. For a 60-second answer, use a busybox initramfs:

```bash
mkdir -p ~/work/rootfs && cd ~/work/rootfs
mkdir -p {bin,sbin,etc,proc,sys,dev,tmp,root,lib,lib64,usr/{bin,sbin}}
cp /bin/busybox bin/
for cmd in sh ls cat echo mount umount mkdir insmod rmmod dmesg lsmod modinfo poweroff; do
    ln -sf busybox bin/$cmd
done
cat > init <<'EOF'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev
echo "ldd-textbook rootfs ready"
exec /bin/sh
EOF
chmod +x init
find . | cpio -o -H newc | gzip > ../initramfs.cpio.gz
```

(Get `busybox-static` from your package manager if `/bin/busybox` isn't statically linked.)

## 5. Boot it

```bash
qemu-system-x86_64 \
    -enable-kvm -cpu host -smp 4 -m 2G \
    -kernel ~/work/linux/arch/x86/boot/bzImage \
    -initrd ~/work/initramfs.cpio.gz \
    -append "console=ttyS0 nokaslr" \
    -nographic
```

`Ctrl-A x` to quit.

`nokaslr` is important if you want gdb addresses to match `vmlinux`.

## 6. Attach gdb to the running kernel

In the QEMU command, add `-s -S` (`-s` = gdbserver on :1234, `-S` = freeze at start). In another terminal:

```bash
cd ~/work/linux
gdb vmlinux
(gdb) target remote :1234
(gdb) hbreak start_kernel
(gdb) c
```

You're now stepping through the Linux boot. Welcome to god mode.

## 7. Add the QEMU `edu` device for the PCI lab

```bash
qemu-system-x86_64 ... \
    -device edu \
    -device intel-iommu                  # optional, exercises IOMMU
```

Inside the guest you'll see the `edu` PCI device. Lab 06 binds against it.

## 8. Enable AMD GPU passthrough (optional, advanced)

For real `amdgpu` work you can pass through a discrete GPU using VFIO. This is a longer setup, covered in Chapter 50. For most learning, the QEMU `edu` device + a real machine running `amdgpu` natively is enough.

## 9. Snapshots

Always make a snapshot of a working VM image before you go destructive:

```bash
qemu-img create -f qcow2 disk.qcow2 8G
qemu-img snapshot -c clean disk.qcow2
qemu-img snapshot -a clean disk.qcow2     # restore
```

## 10. The 30-second iterate loop

Once set up, the developer cycle is:

```bash
make -j$(nproc) bzImage modules           # rebuild
sudo make modules_install INSTALL_MOD_PATH=$ROOTFS
# rebuild initramfs (or use 9p / virtfs to share)
qemu-system-x86_64 ... -kernel … -initrd …
# inside guest: insmod / dmesg / test
# crash? keep dmesg, exit, edit code, repeat
```

If a change takes more than 30 seconds to test, fix the workflow before fixing the bug.
