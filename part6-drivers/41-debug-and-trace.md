# Chapter 41 — debugfs, sysfs, tracing, ftrace, perf

You will spend more hours debugging than coding. The kernel has world-class instrumentation; learning to wield it is a force multiplier.

## 41.1 dmesg / printk

Default output channel. Use `pr_info`, `pr_err`, `dev_info`, `drm_info`. Levels at top (Chapter 29).

```bash
dmesg -w        # follow new messages
dmesg -T        # human time
dmesg -l err,warn   # filter
journalctl -k   # systemd's kmsg view
```

Boot-time `loglevel=8` raises console verbosity.

`dynamic debug` lets you toggle individual `pr_debug`s at runtime:

```bash
echo 'file amdgpu_cs.c +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
```

## 41.2 sysfs vs debugfs

| | sysfs (`/sys`) | debugfs (`/sys/kernel/debug`) |
|---|---|---|
| Stable ABI? | Yes — versioned, documented | No — develop-only |
| One value per file | Yes | No (anything goes) |
| Use for | User config, status | Driver dev, deep inspection |
| Helpers | `DEVICE_ATTR_*`, `sysfs_emit` | `debugfs_create_*` |

debugfs example:

```c
static int amdgpu_fence_info_show(struct seq_file *m, void *p)
{
    seq_printf(m, "current seqno %u\n", current_seqno);
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(amdgpu_fence_info);

debugfs_create_file("fence_info", 0444, parent, dev, &amdgpu_fence_info_fops);
```

amdgpu has dozens of debugfs files — fence rings, BO lists, pm voltage tables, register dumps.

## 41.3 ftrace

Kernel function tracer. The most powerful day-to-day tool.

```bash
sudo trace-cmd record -p function_graph -l 'amdgpu_*'
# … reproduce …
sudo trace-cmd stop
sudo trace-cmd report > trace.txt
```

You see every call into amdgpu, indented by depth. For a hung path, this is gold.

Higher-level: `tracepoint`s — events drivers emit at named points. amdgpu adds tracepoints in `amdgpu_trace.h`. View with:

```bash
sudo trace-cmd list -e amdgpu
sudo trace-cmd record -e amdgpu_cs
```

## 41.4 perf

CPU profiler. Used to find hotspots:

```bash
sudo perf record -F 99 -ag -- sleep 5
sudo perf report
sudo perf script | flamegraph.pl > flame.svg
```

For driver work, attach to your process or system-wide. `perf top` for live.

`perf trace` is `strace` on perf — much faster.

## 41.5 BPF / bpftrace

```bash
sudo bpftrace -e 'tracepoint:amdgpu:amdgpu_cs { @[comm] = count(); }'
```

One-liners for dynamic tracing. `bcc` and `bpftrace` are essential for production tracing.

## 41.6 GDB on the kernel

For VM-based dev:

```bash
qemu … -s -S
gdb vmlinux
(gdb) target remote :1234
(gdb) hbreak start_kernel
(gdb) c
```

For real machines, **kgdb** over serial. Set up early or you'll never reach a kernel panic.

## 41.7 crash and pstore

When a panic happens on a real machine you have:

- **kdump**: another kernel boots and saves the crashed kernel's memory; analyze with `crash`.
- **pstore**: persistent ramoops; reads dmesg from RAM after reboot.

You will need these once you hit a real GPU panic on a customer machine. AMD's bug tracker has many "please attach kdump."

## 41.8 KASAN, KCSAN, lockdep, DEBUG_VM

Already mentioned. Build with these on for development. They catch bugs you'd otherwise spend days hunting.

## 41.9 Memory leak detection

- **kmemleak** — `CONFIG_DEBUG_KMEMLEAK=y`. Scans memory for orphaned allocations periodically.

```bash
echo scan | sudo tee /sys/kernel/debug/kmemleak
sudo cat /sys/kernel/debug/kmemleak
```

Best when you can reliably reproduce.

## 41.10 amdgpu-specific debug knobs

Module parameters (boot or rmmod/insmod) controlling amdgpu:

```
amdgpu.debug_mask=0x...
amdgpu.runpm=0      # disable runtime PM
amdgpu.dpm=0        # disable DPM
amdgpu.gpu_recovery=2
amdgpu.mes=0        # disable MES (microcontroller scheduler)
```

debugfs files:

- `/sys/kernel/debug/dri/0/amdgpu_pm_info`
- `/sys/kernel/debug/dri/0/amdgpu_gpu_recover` — write 1 to force a reset.
- `/sys/kernel/debug/dri/0/amdgpu_fence_info`
- `/sys/kernel/debug/dri/0/amdgpu_gem_info`
- `/sys/kernel/debug/dri/0/amdgpu_vm_info`

When triaging a hang, dump these.

## 41.11 ROCm-side tools

- `rocm-smi` — voltage, temperature, fan, clocks.
- `rocminfo` — discovery.
- `rocprof` — kernel-level profiler for HIP.
- `rocgdb` — GDB for AMDGPU device code.

You'll cross between kernel and userspace tools constantly.

## 41.12 Reading dmesg traces

A typical amdgpu hang trace:

```
[  XXX.XXX] amdgpu 0000:01:00.0: [drm:amdgpu_ring_test_helper] *ERROR* ring sdma0 timeout
[  XXX.XXX] amdgpu 0000:01:00.0: [drm] GPU recovery starts
[  XXX.XXX] amdgpu 0000:01:00.0: [drm] failed to suspend display engine
[  XXX.XXX] Call Trace:
[  XXX.XXX]  ? amdgpu_device_gpu_recover.cold+...
[  XXX.XXX]  ? drm_sched_main+...
```

The `Call Trace` is the kernel stack at panic. Resolve symbol offsets to source line with:

```bash
addr2line -e vmlinux 0xffffffffc04a5e3a
```

Or use `gdb`:

```bash
gdb vmlinux
(gdb) list *(amdgpu_device_gpu_recover+0x1f8)
```

Mastery of stacks → mastery of triage.

## 41.13 Exercises

1. Enable dynamic_debug on your driver; toggle pr_debug live.
2. Add a tracepoint to your edu driver: `TRACE_EVENT(edu_factorial, TP_ARGS(u32 input, u32 output), …)`. Use `trace-cmd record -e edu_factorial`.
3. Run `perf record -ag` on a stress workload of your driver; identify the hot function.
4. Build a kernel with KASAN+KMEMLEAK; introduce a leak; observe.
5. Write down the contents of three amdgpu debugfs files on your machine. Identify what each tells you.

---

End of Part VI. Move to [Part VII — The Linux GPU stack (DRM + amdgpu)](../part7-gpu/42-gpu-architecture.md).
