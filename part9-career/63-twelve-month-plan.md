# Chapter 63 — The 12-month plan from contractor to FTE

Concrete, week-by-week. Adjust to your starting level. The goal: by month 12 you have an FTE offer in hand or you're being prepared for one.

## Months 1–2: foundations

**Goal**: write fluent C and confident kernel modules.

- Re-read Parts I–II of this book; do every exercise.
- Build a custom kernel; load `hello.ko`; modify init message in `start_kernel`.
- Implement: arena allocator, free-list allocator, generic linked list with `container_of`.
- Build, run, and load the chardev skeleton (Part VI Chapter 35) on real hardware.
- Pass through 5 of these exercises *each week*: small, real, tracked.

**Output**: github repo with five small kernel modules, clean READMEs.

## Months 3–4: kernel and concurrency

**Goal**: confident with the kernel's concurrency, memory, and IRQ.

- Re-read Parts III–V; exercises.
- Build the producer/consumer chardev with proper blocking I/O, poll, ioctl, mmap.
- Build the QEMU-`edu` PCI driver from Chapter 37; extend with DMA.
- Read the Linux kernel coding style cover-to-cover. Run `checkpatch.pl` on everything you write.
- **Subscribe to amd-gfx@lists.freedesktop.org**; read patches every day.

**Output**: a "real" PCI driver with DMA + IRQ + chardev.

## Months 5–6: DRM and amdgpu orientation

**Goal**: read amdgpu source comfortably; submit your first upstream patch.

- Re-read Part VII through Chapter 49; do exercises.
- Read every `amdgpu_*.c` file's intro and one function from each.
- Pick one IP block (e.g. `gfx_v10_0.c`); spend a week reading.
- Build Mesa from source; run `vkcube` and `vulkaninfo` against your build.
- **Submit your first kernel patch upstream.** Even a typo. Get it merged.

**Output**: one merged patch with your `Signed-off-by`.

## Months 7–8: ownership area

**Goal**: become the local expert on a specific subsystem.

- Choose one: `amdgpu_cs` path, `amdgpu_vm`, reset/recovery, RAS, runtime PM, debugfs.
- Read every line. Take notes. Diagram on whiteboard.
- Find three open issues in that area; reproduce; comment with analysis.
- Pair with a senior engineer; offer to do "boring fixes" they don't want.
- Read every commit to that area for the last 6 months on `git log`.

**Output**: written internal doc on the subsystem; first non-trivial upstream patch.

## Months 9–10: real bug ownership

**Goal**: end-to-end ownership of one customer-impacting bug.

- Pick an open issue, ideally with multiple comments and no resolution.
- Reproduce locally. Debug systematically. Capture full state on each attempt.
- Communicate: status updates to manager weekly.
- Land the fix upstream.

The first end-to-end bug ownership is the single biggest career step in the first year. It is the proof that you can be trusted with real work.

## Months 11–12: convert

**Goal**: signal FTE-readiness; convert.

- Patches landed: aim for 5+ upstream this year, of varying sizes.
- Review at least 10 patches from peers (publicly, on the list).
- Write one polished internal design doc proposing a feature improvement.
- Have a 1:1 with your manager: "I want FTE conversion. What's needed?"
- Know your salary band and the full comp structure before negotiation.

The conversation with your manager *should not be a surprise* to them. By now you've shipped, you've reviewed, you're known, you've helped. The req is a formality.

## A weekly cadence

Every week, even slow weeks:

- **Read patches** (1 hr).
- **Read source** (1-2 hr).
- **Write code** (3-5 hr).
- **Test** (1-2 hr).
- **Review** at least one peer patch.
- **Triage** one bug, even if you don't own it.

That's 8-12 hours of "growth" work. Add to your normal job duties.

## On burnout

Driver work can be punishing. Bugs that take weeks. Reset paths that hurt your soul. Reviewers who nitpick. A few rules:

- **Take real weekends**. Reading patches "just for fun" still counts as work.
- **Own one thing at a time** to the depth you can sustain.
- **Rotate areas** when one becomes oppressive.
- **Talk** to your manager when stuck. Better than spinning silently.

## On asking for help

When stuck:

- Spend at most **a day** spinning solo.
- Then write a focused question: what you're seeing, what you've tried, what hypotheses you have.
- Send to whoever is the relevant expert (use #channel or person directly).
- Almost always: 80% of help comes from formulating the question well.

## On luck

There's a luck component: which team you join, who your manager is, which silicon ships. You can stack the deck:

- **Pick teams** with throughput. (Visible commits, working products.)
- **Pick managers** known for promoting their reports.
- **Pick areas** that matter to the company (not pet projects).
- **Be patient** with the actual hire/promote cycles. Many requisitions are 3-6 months from "approved" to "open."

## Final words

Two years from now you should:

- Have ~50 upstream commits.
- Be the person someone else asks "how does X work?"
- Have shipped a real feature in a real release.
- Be on a hiring panel for new hires.
- Be paid well, sleeping well, learning fast.

That's the destination this book aimed at.

You have everything you need. The path is clear. **Now go build.**

---

End of Part IX. Move to [Labs](../labs/) for hands-on exercises, or to [Appendix](../appendix/) for cheat sheets.
