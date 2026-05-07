# Lab 05 — Ring-buffer character device

Full chardev with read, write, poll, and ioctl. Same template every DRM driver starts from.

## Build & load

```bash
make
make load
```

## Use

```bash
./testtool state                 # head=0 tail=0 capacity=4096
./testtool write "hello AMD"
./testtool state                 # head=9 tail=0 ...
./testtool read 5                # read 5 bytes
./testtool reset
echo from-shell > /dev/myring
cat /dev/myring                  # blocks if empty
```

Try `cat /dev/myring &` then `echo hi > /dev/myring`. The cat will print and exit.

## Stretch

1. Add `mmap` exposing the buffer to userspace; benchmark vs. `read`.
2. Make it a multi-instance device (multiple `/dev/myring0`, `/dev/myring1`).
3. Add a kthread that periodically writes a timestamp; readers consume.
