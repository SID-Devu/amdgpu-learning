# Chapter 58 — Debugging: gdb, kgdb, crash, ftrace, dmesg, lockdep

This is the toolbox you'll reach for when something is broken. Master each.

## 58.1 dmesg first, always

The kernel speaks via dmesg. Read every word of the relevant section.

Look for:

- **Call traces** — kernel stack at oops/warning.
- **`KASAN` reports** — memory bugs.
- **`Lockdep` reports** — locking bugs.
- **`amdgpu`** prefixes — driver-specific.

Save the full dmesg before any reboot or rmmod for analysis.

## 58.2 gdb on userspace

```bash
gdb --args ./your_program arg1 arg2
(gdb) run
(gdb) bt
(gdb) print var
(gdb) frame N
(gdb) up / down
(gdb) info threads
(gdb) thread 3
(gdb) watch *(int*)0x1234
```

For C++:

- `(gdb) p/r obj` — raw, skip pretty-printer.
- Auto-load libstdc++ pretty-printers in your `.gdbinit`.

## 58.3 gdb on the kernel (under QEMU)

```bash
qemu-system-x86_64 -kernel bzImage -append "nokaslr" -s -S
gdb vmlinux
(gdb) target remote :1234
(gdb) break start_kernel
(gdb) c
```

`-S` stops on entry; `-s` opens GDB stub on port 1234. `nokaslr` matters because gdb uses absolute addresses.

Set `lx-symbols` and other helpers from `scripts/gdb/`.

## 58.4 kgdb on real hardware

Boot kernel with `kgdboc=ttyS0,115200` and `kgdbwait`. On serial host, `gdb vmlinux; target remote /dev/ttyS0`. Step through real machines.

Less convenient than VM but sometimes the only way for hardware-specific bugs.

## 58.5 crash and kdump

When a kernel panics:

```
echo "kdump_panic = 1" >> /etc/default/kdump   # distro-specific
service kdump start
```

After a panic, the second kernel saves `/proc/vmcore`. Analyze:

```bash
sudo crash vmlinux /var/crash/vmcore
crash> bt
crash> log
crash> ps
crash> mod -d            # list modules
```

`crash` is GDB-on-steroids for postmortem.

## 58.6 ftrace for kernel function tracing

```bash
echo function_graph > /sys/kernel/tracing/current_tracer
echo 'amdgpu_*' > /sys/kernel/tracing/set_ftrace_filter
echo 1 > /sys/kernel/tracing/tracing_on
# … reproduce …
echo 0 > /sys/kernel/tracing/tracing_on
cat /sys/kernel/tracing/trace > my.trace
```

Or use `trace-cmd`:

```bash
sudo trace-cmd record -p function_graph -l 'amdgpu_*'
sudo trace-cmd report > log.txt
```

For a hung path, ftrace is gold. You see every entry/exit; if a function never returns, the indent never closes — that's your hang.

## 58.7 lockdep, KASAN, KCSAN, KMSAN, UBSAN, KFENCE, DEBUG_VM

Build with these on for development. Each finds a specific bug class:

- **lockdep** — lock ordering and irq-safety.
- **KASAN** — use-after-free, OOB.
- **KFENCE** — sampled, low-overhead OOB.
- **KCSAN** — data races.
- **KMSAN** — uninitialized reads (clang only).
- **UBSAN** — undefined behavior.
- **DEBUG_VM** — invariant checks in MM.
- **DEBUG_KOBJECT** — sysfs lifetime.

Boot with all of them on at least once before sending a patch series.

## 58.8 Driver-specific: amdgpu

- `amdgpu.debug_mask=0x...` — turn on dev-only logging.
- `/sys/kernel/debug/dri/0/amdgpu_*` — internal state.
- `umr` — register-level dump tool. `sudo umr -ls` lists rings; `sudo umr -gr 0 0 0x6118` reads register.
- `amdgpu_gpu_recover` — force a reset.

For hangs:

1. Capture `dmesg` (full).
2. Dump `/sys/kernel/debug/dri/0/amdgpu_fence_info`, `_gem_info`, `_vm_info`.
3. `umr -O halt_waves; umr -wa -O wave_status -i ...` — examine in-flight waves.
4. Submit bug report with above.

## 58.9 Reproducing intermittent bugs

- **Run the test in a tight loop** with logging.
- **Increase concurrency** to compress timing windows.
- **Add `udelay`** in suspicious paths to widen race windows.
- **Use KCSAN** for data races.
- **Use `ftrace`** to pinpoint when the divergence began.

A bug you can't reproduce is a bug you can't fix.

## 58.10 Reading core dumps

Userspace SIGSEGV → core dump (if `ulimit -c unlimited`):

```bash
gdb ./your_app core.12345
(gdb) bt
(gdb) info threads
```

For amdgpu-induced segfaults from misuse of libdrm, the core often points right at the bad ioctl call site.

## 58.11 Static analysis

- `sparse` — kernel's own. Catches `__user`/`__iomem` mismatches, endian bugs.
- `clang --analyze`, `clang-tidy`.
- `cppcheck`.
- `coccinelle` (`make coccicheck`) — semantic patches.

Each finds different bugs. Run them periodically.

## 58.12 The triage checklist

When a bug lands on your desk:

1. Reproduce locally?
2. dmesg full, before+after?
3. lspci, kernel version, distro?
4. Last known good kernel?
5. Simple reproducer?
6. `git bisect` if regression.
7. ftrace, then bpftrace, then crash dump if needed.

A clean triage report goes a long way to getting the right person to look.

## 58.13 Exercises

1. Boot a debug kernel (KASAN+lockdep+DEBUG_VM). Insmod a module that intentionally takes locks in two orders; observe lockdep.
2. Cause a userspace SEGV; load core in gdb; navigate frames.
3. Trace your edu driver with ftrace; observe the call sequence on a write/IRQ.
4. Set up kdump on a real machine; force a panic with `echo c > /proc/sysrq-trigger`; reboot; analyze with `crash`.
5. Read `Documentation/admin-guide/bug-hunting.rst`. Internalize.

---

End of Part VIII. Move to [Part IX — Getting hired (AMD / NVIDIA / Qualcomm / Intel)](../part9-career/59-what-they-want.md).
