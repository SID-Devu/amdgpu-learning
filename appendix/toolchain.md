# Appendix — Toolchain cheat sheet

Quick reference for the daily commands.

## Build

```bash
gcc -std=c11 -Wall -Wextra -Werror -O2 -g    # release
gcc -std=c11 -Wall -Wextra -Werror -O1 -g \
    -fsanitize=address,undefined             # dev
g++ -std=c++17 -Wall -Wextra -Werror -O2 -g
clang -fsanitize=memory -fno-omit-frame-pointer  # MSan (uninit reads)
```

## Inspect

```bash
nm a.out                          # symbols (T = text, U = undef, …)
nm -C a.out                       # demangle C++
nm -D libfoo.so                   # dynamic symbols only
objdump -d a.out                  # disassemble
objdump -h a.out                  # sections
readelf -a a.out
strings a.out | head
file a.out
ldd a.out                         # dynamic deps
addr2line -e vmlinux 0xfff…       # symbol + line for an address
c++filt _ZNK3std9basic_…          # demangle one symbol
```

## Debug

```bash
gdb --args ./prog arg1            # start gdb
(gdb) run / r                     # start
(gdb) break / b main              # set breakpoint
(gdb) bt                          # backtrace
(gdb) info threads / thread N     # threads
(gdb) print x / p x               # print
(gdb) watch *(int*)0xdead         # watchpoint
(gdb) finish                      # run until function returns
(gdb) layout asm/regs/src         # tui

valgrind --tool=memcheck ./prog
valgrind --tool=helgrind ./prog   # data races
strace -f -e ioctl ./prog
ltrace ./prog
perf stat ./prog
perf record -ag -- ./prog
perf report
perf top
```

## Kernel build

```bash
cp /boot/config-$(uname -r) .config
make olddefconfig
scripts/config --enable DEBUG_KERNEL DEBUG_INFO PROVE_LOCKING KASAN
make olddefconfig
make -j$(nproc)
sudo make modules_install
sudo make install
```

## Out-of-tree module Makefile

```make
obj-m := mymod.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

## Useful kernel debug knobs

```bash
echo 1 > /proc/sys/kernel/sysrq
dmesg -w
echo 'file amdgpu* +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
echo function_graph > /sys/kernel/tracing/current_tracer
echo 'amdgpu_*' > /sys/kernel/tracing/set_ftrace_filter
echo 1 > /sys/kernel/tracing/tracing_on
```

## git daily

```bash
git status
git diff
git diff --stat HEAD~1
git log --oneline -20
git log --graph --oneline --all
git blame file.c
git bisect start; git bisect bad; git bisect good <sha>
git rebase -i HEAD~5
git commit -s -m "subject"     # -s for sign-off
git format-patch -1 -o out/
git send-email --to=user out/0001-*.patch
git cherry-pick <sha>
```

## DRM/amdgpu

```bash
lspci -vv -s 01:00.0
cat /sys/class/drm/card*/status
cat /sys/kernel/debug/dri/0/name
cat /sys/kernel/debug/dri/0/amdgpu_pm_info
echo 1 > /sys/kernel/debug/dri/0/amdgpu_gpu_recover    # force reset
glxinfo | grep -i 'renderer string'
vulkaninfo --summary
modetest -M amdgpu                  # libdrm test tool
```

## ROCm

```bash
rocm-smi
rocminfo
clinfo
rocprof --hip-trace ./hip_app
```
