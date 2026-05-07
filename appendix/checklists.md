# Appendix — Checklists you can paste into your dotfiles

These are the lists I personally walk down when I'm tired and the build is broken at 2 AM.

## "I just hit a kernel oops" checklist

1. Save the full `dmesg`. Don't trust scrollback; pipe to a file: `dmesg > /tmp/oops.log`.
2. Note: PID, comm, RIP, CR2 (faulting address), call trace.
3. Decode the RIP: `./scripts/decode_stacktrace.sh vmlinux < oops.log`.
4. `addr2line -e vmlinux <RIP>` for file:line.
5. Look at the **last function in your driver** in the trace. That's almost always the bug site.
6. Reproduce with KASAN if not enabled. KASAN turns "oops" into "exact bad access + which alloc/free pair."
7. If you can't reproduce: enable `lockdep`, `KCSAN`, `slub_debug=FZP`. Reboot. Try again.

## "I just hit a hang" checklist

1. Magic SysRq:
   - `Alt+SysRq+t` → all task stacks to dmesg.
   - `Alt+SysRq+l` → all CPU stacks.
   - `Alt+SysRq+w` → blocked tasks (uninterruptible sleep).
2. From sysrq output, find the offending CPU/task. Look for `mutex_lock`, `wait_for_completion`, `schedule`.
3. Check the GPU: `cat /sys/kernel/debug/dri/0/amdgpu_*ring*` — are all rings idle?
4. `cat /proc/<pid>/stack` of the hung process.
5. If it's a deadlock, `lockdep` would have caught it on a debug build → make sure you're running a debug build.

## "I'm about to send a kernel patch" checklist

- [ ] One logical change per patch.
- [ ] Built it. (`make C=1 W=1` for sparse + extra warnings on touched files.)
- [ ] Tested it. (Mention exactly what test in the commit msg.)
- [ ] `scripts/checkpatch.pl --strict` clean (or justified).
- [ ] `scripts/get_maintainer.pl` to get To/Cc list.
- [ ] Subject is `subsystem: short imperative summary` (≤72 chars).
- [ ] Body explains *why* not *what*.
- [ ] `Signed-off-by:` line.
- [ ] `Fixes:` tag if it fixes a known commit (use `--abbrev=12`).
- [ ] `Cc: stable@vger.kernel.org` if user-visible bugfix worth backporting.
- [ ] No churn in unrelated files.
- [ ] Sent with `git send-email` (not GitHub PR, not attachment).
- [ ] Reply with v2/v3 in the same thread; include changelog under `---`.

## "I'm reviewing someone's patch" checklist

- [ ] Apply locally with `git am`. Build it. Skim runtime output if possible.
- [ ] Read the *whole* changed file, not just the hunk.
- [ ] Locking: every shared field touched, who holds what?
- [ ] Memory: who frees, on which path? Error paths cleaned up?
- [ ] Concurrency: can this fire in interrupt context? On another CPU during teardown?
- [ ] DMA/coherence: is the right sync called? Direction right?
- [ ] Userspace ABI: never break it. Never.
- [ ] Comments: necessary, accurate, not narrating.
- [ ] Reply on-list. Be technical, not personal. Quote specifically.

## "I'm starting at AMD on Monday" checklist (Day 0)

- [ ] Kernel built from source from git, booting in a VM.
- [ ] AMD GPU in a desktop, latest distro kernel + Mesa, all userspace stacks (OpenGL/Vulkan/ROCm) verified working.
- [ ] `umr` installed and `umr -lb` runs.
- [ ] `git clone` of `linux`, `mesa`, `drm`, `libdrm`, `llvm-project` somewhere on disk.
- [ ] Read your team's onboarding docs end-to-end before Day 1.
- [ ] Email + git + IRC/Slack working. Sign-off + corp legal name in `git config`.
- [ ] One pre-built tracing/printing helper in `~/bin/` (e.g., a script that turns on dynamic_debug for amdgpu).
- [ ] LinkedIn says "Software Engineer, AMD" with the start date. (Soft thing, but the reality of FTE conversion is influenced by visibility.)

## "I'm building a portfolio commit" checklist

- [ ] Solves a real bug or adds a real feature, not a toy.
- [ ] Commit message is interview-quality (problem, root cause, fix, impact, test).
- [ ] Test plan included (output, screenshot, perf number).
- [ ] No trailing whitespace, no debugging prints left over.
- [ ] Tagged in your portfolio repo's README with the *upstream* link.
- [ ] You can talk about it for 10 minutes without notes.

Print these. Tape them above your desk.
