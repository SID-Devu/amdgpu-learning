# USP 12 — Userspace performance & debug tools

> If you can't measure it, you can't fix it. Master these tools.

## strace — trace system calls

```bash
strace ./prog                     # all syscalls
strace -e trace=openat ./prog     # only opens
strace -f ./prog                  # follow forks/threads
strace -p PID                     # attach to running
strace -c ./prog                  # summary (count, time)
strace -tt -T ./prog              # timestamps and durations
strace -o log.txt ./prog          # write to file
```

If your program "hangs," strace tells you on which syscall. If it "fails silently," strace shows the errno.

Limit to interesting syscalls or you drown in `read`/`write`/`mmap`.

## ltrace — trace library calls

```bash
ltrace ./prog
```

Shows libc calls: `malloc`, `printf`, `strcpy`, etc. Useful for "what's this program asking glibc to do?"

## perf — Linux's profiler (the most important tool)

### Counters
```bash
perf stat ./prog                    # basic counters: instructions, cycles, IPC
perf stat -e cache-misses,cache-references,page-faults ./prog
perf list                           # available events
```

### CPU profile
```bash
perf record ./prog                  # sample call stacks
perf report                         # interactive viewer
perf report --stdio                 # text dump
```

For full call graphs:
```bash
perf record -g -- ./prog
perf report -g graph --no-children
```

### Top, live
```bash
perf top -p PID
```

### Trace events
```bash
perf trace ./prog                   # like strace but lower overhead
```

### Flame graphs (read these on a wall)
```bash
perf record -g -F 99 -- ./prog
perf script | ./stackcollapse-perf.pl | ./flamegraph.pl > out.svg
```

Brendan Gregg's flame graphs (https://www.brendangregg.com/flamegraphs.html) — every senior performance engineer uses them.

## valgrind — memory error detector

```bash
valgrind ./prog
valgrind --leak-check=full --show-leak-kinds=all ./prog
```

Catches:
- Use-after-free.
- Double-free.
- Buffer overruns/underruns.
- Uninitialized memory reads.
- Memory leaks at exit.

10–50× slower than native, but priceless for catching memory bugs.

Alternative: **AddressSanitizer (ASAN)**, faster (2–3× slowdown), built into gcc/clang:
```bash
gcc -fsanitize=address -g prog.c -o prog
./prog
```

Use ASAN by default. Valgrind for the cases ASAN misses (rare).

## ThreadSanitizer (TSan)

```bash
gcc -fsanitize=thread -g -O1 prog.c
```

Catches data races, deadlocks. Slower (5–10×).

## UndefinedBehaviorSanitizer (UBSan)

```bash
gcc -fsanitize=undefined -g prog.c
```

Catches signed integer overflow, shift overflow, division by zero, null deref, etc. Often free in CPU; always enable in CI.

## gdb — the debugger

### Basics

```bash
gdb ./prog
(gdb) run
(gdb) break main
(gdb) break filename.c:42
(gdb) break funcname
(gdb) run arg1 arg2
(gdb) next            # n: step over
(gdb) step            # s: step into
(gdb) continue        # c
(gdb) finish          # run to return
(gdb) print var       # p var
(gdb) print *ptr@10   # 10-element array
(gdb) info locals
(gdb) backtrace       # bt
(gdb) frame 3
(gdb) watch var       # break when var changes
(gdb) info threads
(gdb) thread 3
(gdb) quit
```

### Crash analysis
```bash
ulimit -c unlimited
./prog                           # crashes, leaves a core file
gdb ./prog core
(gdb) bt
```

### Attach to running
```bash
gdb -p PID
```

### Useful commands
- `pretty print`: `(gdb) set print pretty on`
- `tui`: `(gdb) layout src` — split view.
- `rr` (record + replay) — astonishing tool for "I can step backwards to find the bug."

Add `gdbinit` (`~/.gdbinit`) for shortcuts and Python pretty-printers.

## addr2line — convert addresses to source

```bash
addr2line -e prog 0x401234
```

Used when you have a stack trace from a crash.

## objdump / readelf / nm — inspect binaries

```bash
readelf -h prog                    # ELF header
readelf -s prog                    # symbols
nm prog | head                     # symbols simplified
objdump -d prog | less             # disassembly
objdump -h prog                    # section headers
file prog                          # what is this file
strings prog | head                # printable strings
```

Useful for "is this stripped?", "what symbols does it export?", "what does this assembly do?"

## ltrace, ldd

`ldd prog` — what shared libraries does this binary need?
`nm prog | grep ' U '` — undefined symbols (resolved at runtime).

## time — basic wallclock

```bash
time ./prog
```

```
real    0m3.241s
user    0m2.950s
sys     0m0.180s
```

`real` = wallclock; `user` = CPU in user mode; `sys` = CPU in kernel. `(user + sys) / real` ≈ degree of parallelism.

## /usr/bin/time -v

More detail: peak RSS, page faults, voluntary/involuntary context switches.

## ftrace, eBPF, bcc, bpftrace — kernel-side tracing

These work mostly on kernel-level events; we'll cover them in Part V/VIII. Mention here:

- `trace-cmd record / report` — wraps ftrace.
- `bpftrace -e 'tracepoint:syscalls:sys_enter_openat { printf("%s\n", str(args->filename)); }'`
- `bcc` tools (`opensnoop`, `tcpconnect`, `execsnoop`, etc.).

These let you trace **anything** in the kernel, low overhead, without rebuilding.

## htop, iotop, iostat, vmstat

System-level health:
- `htop` — colorful top.
- `iotop` — disk I/O per process.
- `iostat -xz 1` — per-disk throughput, await, util.
- `vmstat 1` — paging, CPU, IO summary.
- `ss -ant` — TCP states.
- `dstat` — combined.

## /proc/PID/wchan — what's it waiting on?

```bash
cat /proc/PID/wchan
```

Prints kernel function it's blocked in. Great for "why is it stuck?"

## A debugging mindset (the 30-minute rule again)

When you hit a problem:

1. **Reproduce reliably.** A 1-in-100 bug is a nightmare; reproduce often if possible.
2. **Form a hypothesis.** "I think X is failing because Y."
3. **Test it cheaply.** strace, print, single test.
4. **Read the failing code carefully.** Often the bug is on the line you already read.
5. **Bisect.** When did it start working/stop working? `git bisect`. Or remove half the inputs.
6. **Use a sanitizer.** ASAN finds bugs your eye won't.
7. **Get a smaller test case.** A 5-line repro is gold.

## Try it

1. Compile a buggy program (use-after-free) under ASAN. Verify it catches the bug.
2. Profile a CPU-heavy program (e.g., dumb merge sort on 10M ints) with `perf record`. Identify the hot function.
3. Run the same with TSan (must be threaded). Try one with deliberately racing threads.
4. Crash a program with NULL deref. Get the core file. Use gdb to print the crashing line.
5. Use `bpftrace -e 'tracepoint:syscalls:sys_enter_open* { @[comm]=count(); }'` for 30 seconds. See which programs open the most files.
6. Use `strace -c` on a few different programs. Compare which syscalls dominate.

## Read deeper

- *Systems Performance: Enterprise and the Cloud* by Brendan Gregg — the bible for performance work.
- *BPF Performance Tools* by Brendan Gregg.
- "Mistakes Engineers Make in Large Established Codebases" by Will Larson — broader engineering wisdom.

This finishes Part USP. Next stage in the [LEARNING PATH](../LEARNING-PATH.md):

- **Stage 6**: [C++ for systems](../part2-cpp/) (3 weeks)
- **Stage 7**: [Multithreading deep](../part4-multithreading/) (4 weeks)
- **Stage 8**: [OS theory deep — Part OST](../partOST/README.md) (4–6 weeks) — coming up next.

Then the kernel.
