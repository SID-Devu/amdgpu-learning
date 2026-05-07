# Lab 04 — Hello kernel module

Your first kernel module with parameters.

## Build & load

```bash
make
make load
make dmesg     # see "hello, AMD (0)" etc.
make unload
make dmesg     # see "goodbye, AMD"
```

If the build complains it can't find headers:

```bash
sudo apt install linux-headers-$(uname -r)
```

## Stretch

1. Add a `module_param_array` for an array of names.
2. Make it create a sysfs file under `/sys/module/hello/parameters/` you can read/write at runtime; observe the value live-update.
3. Use `EXPORT_SYMBOL` and have a *second* module call into this one. Observe `lsmod`'s "Used by" column.
