# Chapter 61 — Projects that get you hired

Resumes with hardware/kernel projects pass screens that résumés with "built a CRUD app in Django" don't. Here is a graded list, easiest first, all relevant to the AMD/NVIDIA/Qualcomm pipeline.

## 61.1 Tier-0 (you must have at least these)

1. **Custom Linux kernel build + boot** — your own bzImage with a `pr_info("hi from <name>")` in `start_kernel`. Document on github.
2. **Out-of-tree hello world `.ko`** — Chapter 28-29 builds it; productionize.
3. **Char driver with read/write/poll/ioctl/mmap** — Chapter 35-36. Add a userspace test program.
4. **PCI driver against QEMU's `edu`** — Chapter 37; with full DMA support.
5. **Userspace libdrm_amdgpu submission program** — opens `/dev/dri/renderD*`, builds an IB, submits, reads result.

If you've done all five, you've passed the bar of "actually wrote driver code."

## 61.2 Tier-1 (interview gold)

6. **Producer/consumer ring buffer** — both lock-based and lock-free SPSC. Benchmark, write up.
7. **Tiny memory allocator** — arena + free-list. Pass leak/overflow tests under ASan.
8. **Lock-free hash table** — open addressing, linear probing, with hazard pointers or epoch reclamation.
9. **Fictional GPU simulator** — a userspace daemon that pretends to be a GPU; speaks a custom command-buffer language; lets you write a "driver" against it. Forces you to design the API surface.
10. **Tiny KMS-only DRM driver against `vkms`** — enable a virtual scanout, page-flip, write a minimal compositor.

## 61.3 Tier-2 (top-tier)

11. **A real upstream patch in `drivers/gpu/drm/amd/`** — even a typo or a comment cleanup. The `Signed-off-by` is what counts. Walks ahead of 95% of resumes.
12. **A bigger upstream patch** — maybe a missing `__must_check`, an obvious refactor, an extension to debugfs. After your first patch lands, this is straightforward.
13. **Mesa/`radv` patch** — same, on userspace side. Adds a Vulkan extension or fixes a bug.
14. **A blog post or `Documentation/` patch** — writeup of how some part of amdgpu works. Not duplicating existing docs; explain something poorly-documented. Pulls eyeballs and demonstrates deep understanding.
15. **A reproducer + bug report on amd-gfx** for an open issue you triaged.

## 61.4 Tier-3 (signature work)

16. **A new feature in amdgpu** — pick something from the bug tracker labeled enhancement: a new ioctl, a sysfs knob, support for a new chip variant.
17. **A subsystem you understand end-to-end** — e.g. you can explain reset path top-to-bottom, write a slide deck, fix bugs in it.
18. **Performance work** — measure, identify, fix. With before/after numbers.

People who land tier-3 work are FTE within a year.

## 61.5 What to put on github

For each project, a clean repo with:

- **README**: the elevator pitch — what, why, how to build/run.
- **Build instructions** that actually work on a fresh Ubuntu 24.04.
- **Test/example program** showing usage.
- **Design notes** in a `docs/` folder: what you tried, what didn't work.
- **License**: GPL-2.0 for kernel work, MIT/BSD for userspace.

Commits should be small, atomic, with descriptive messages. `git log --oneline` should read like a story.

## 61.6 Anti-patterns (avoid these)

- **AI-generated code without your understanding** — interviews quickly expose it.
- **A 50-step monorepo with one giant commit** — looks like dump.
- **"Cleaned up codebase"** PRs to popular repos with no real change — adds nothing.
- **Buzzword-laden descriptions** — "blockchain DRM driver." Recruiters' eyes glaze.

## 61.7 The resume

For systems / GPU roles, a one-page resume with:

- Contact + linkedin + github at top.
- Education, brief.
- Experience: 3-5 bullets per role. Each bullet should have *measurable impact* ("reduced ioctl latency by 40%") or *technical specificity* ("implemented threaded IRQ + workqueue handler for X subsystem").
- Projects (the github ones above): 2-4 bullets each.
- Skills section short: C, C++, Linux kernel, x86_64/ARM64 asm, gdb, perf, git.
- No "soft skills" cliches.

Resume is for the recruiter to forward, not for the interviewer to read. Give it the strongest first two lines.

## 61.8 The cover letter

Optional. If used:

- 3 paragraphs, max.
- Why this team specifically (named) — show you read about them.
- One concrete piece of evidence for each claimed strength.
- Close with a sentence about what you want to learn there.

For an internal conversion, the "cover letter" is the conversation you have with your manager.

## 61.9 Exercises

1. Build the five Tier-0 projects this month. Push to github with a clean README.
2. Pick one Tier-1 project; spec it; build it; write up.
3. Subscribe to amd-gfx; spend a week reading; then submit a one-line patch fixing something obvious. Get it merged.
4. Refactor your resume to the systems-job template.
5. Write the "bug I'm proudest of" story; trim to 90 seconds spoken.

---

Next: [Chapter 62 — Contributing to the upstream kernel and Mesa](62-upstream-contribution.md).
