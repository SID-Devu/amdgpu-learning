# THE LEARNING PATH — read this first

> **You don't know how to program. That is fine.**
> **You will become an AMD GPU kernel engineer in 18–24 months if you follow this path.**
> **Don't skip stages. Don't read out of order on the first pass.**

The book has many parts. You will not read them all at once and you will not read them in numerical order. **You read them in the order below.** This page is your compass.

## The big picture

```
   Stage 1: Computer literacy        (you can use Linux at all)
        ↓
   Stage 2: Programming basics       (Python + tiny C)
        ↓
   Stage 3: C mastery                (the language of the kernel)
        ↓
   Stage 4: Data Structures & Algorithms   (DSA — for interviews + kernel)
        ↓
   Stage 5: Userspace systems programming   (master Unix as a user)
        ↓
   Stage 6: C++ for systems          (Mesa, ROCm, AMF)
        ↓
   Stage 7: Multithreading & memory models   (atomics, locks, lock-free)
        ↓
   Stage 8: Operating systems theory  (the academic foundation)
        ↓
   ─── you have now mastered userspace + theory ───
        ↓
   Stage 9: Linux kernel foundations  (build it, modules, kernel APIs)
        ↓
   Stage 10: Linux device drivers     (char drivers, PCI, DMA, mmap)
        ↓
   Stage 11: GPU stack — DRM/amdgpu   (the real job)
        ↓
   Stage 12: Hardware/software interface  (caches, PCIe, perf, debug)
        ↓
   Stage 13: Career — get the FTE     (interview, projects, upstream)
        ↓
   Stage 14: Senior / Principal track  (the 24-year arc)
```

Everything before "you have now mastered userspace + theory" is mandatory. Don't enter the kernel until you've earned it.

---

## Stage-by-stage table

Times assume **1 hour/day, 5 days/week**. Double the pace if you do 2 hours/day.

| Stage | Duration | Where |
|---|---|---|
| **1. Computer literacy** | 1 week | [Part 0 ch 0–4](part0-zero/) + [Foundations A4](part0-zero/foundations/) |
| **2. Programming basics (Python + intro C)** | 2 weeks | [Part 0 ch 5–11](part0-zero/) |
| **3. C mastery** | 6 weeks | [Part I (10 chapters)](part1-c/) + [labs 01](labs/01-arena/) |
| **4. Data Structures & Algorithms** | 8–10 weeks | [Part DSA](partDSA/) |
| **5. Userspace systems programming** | 4–6 weeks | [Part USP](partUSP/) |
| **6. C++ for systems** | 3 weeks | [Part II (6 chapters)](part2-cpp/) + [labs 02 + 03](labs/) |
| **7. Multithreading & memory models** | 4 weeks | [Part IV (5 chapters)](part4-multithreading/) + lab 02, 03 deep |
| **8. Operating systems theory** | 4–6 weeks | [Part III (6 chapters)](part3-os/) + [Part OST (10 chapters)](partOST/) |
| **9. Linux kernel foundations** | 6 weeks | [Part V (7 chapters)](part5-kernel/) + [labs 04, 05](labs/) |
| **10. Linux device drivers** | 6 weeks | [Part VI (7 chapters)](part6-drivers/) + [lab 06](labs/06-edu-pci/) |
| **11. GPU stack — DRM/amdgpu** | 8–12 weeks | [Part VII (12 chapters)](part7-gpu/) + [labs 07, 08](labs/) |
| **12. Hardware/software interface** | 3 weeks | [Part VIII (5 chapters)](part8-hw-sw/) |
| **13. Career — get FTE** | parallel from Stage 9 | [Part IX (5 chapters)](part9-career/) |
| **14. Senior / Principal track** | parallel for years | [Part X (29 chapters)](partX-principal/) |

**Total time to be ready for the FTE conversion attempt: 12–18 months at 1 hr/day.**

You can stretch or compress, but **do not skip stages**.

---

## Why this order? (the philosophy)

### "Userspace first, then kernel" — the unbreakable rule

Why?

- **You cannot debug a kernel module if you can't write a 100-line C program.**
- **You cannot reason about driver locking if you can't write multi-threaded userspace code.**
- **You cannot understand `/dev/dri/card0` if you don't understand `/dev/null`, `pipe()`, `read()`, `write()`.**

Every kernel concept has a userspace mirror. Master the mirror first; the kernel is then "just like that, but harder."

### "DSA before kernel" — for two real reasons

1. **Interviews require it.** Every interview at AMD/NVIDIA/Qualcomm has a coding round. They will ask: "implement a hash map." "Reverse a linked list." "Find the kth largest." If you can't do these in 30 minutes in C, you don't get the offer. Period.
2. **The kernel uses these data structures everywhere.** `list_head` (linked list), `rb_root` (red-black tree), `xarray` (radix tree), `hash_table.h` (hashtable), `kfifo` (queue). If you've implemented them yourself once, the kernel reads like prose.

### "OS theory after multithreading" — because…

Multithreading **is** OS theory in practice. Once you've fought race conditions in pthreads, the academic content (Peterson's algorithm, semaphores, dining philosophers, deadlock) makes sense in 1/10th the reading time.

### "GPU last" — because the GPU is just a hard PCI device

The GPU stack assumes:
- you understand C (Stage 3),
- you understand pointers and address spaces (Stage 8),
- you understand kernel modules (Stage 9),
- you understand PCI/DMA/MMIO (Stage 10),
- you understand graphics/compute APIs at least at a conceptual level (Stages 11+).

If any of those are shaky, the GPU chapters will feel impossible. They are not impossible — your foundation is just thin.

---

## Daily rhythm (the secret to actually finishing)

**1 hour/day, 5 days/week**:

```
  ┌─────────────────────────────────────────┐
  │ 0:00 - 0:05  Open notebook. Read your   │
  │              note from yesterday.        │
  ├─────────────────────────────────────────┤
  │ 0:05 - 0:35  Read one new chapter or    │
  │              section. Type every code   │
  │              example.                    │
  ├─────────────────────────────────────────┤
  │ 0:35 - 0:55  Do the exercises / write   │
  │              code. Run it. Break it.    │
  ├─────────────────────────────────────────┤
  │ 0:55 - 1:00  Write 3 sentences in your  │
  │              notebook: "today I"        │
  └─────────────────────────────────────────┘
```

Three days a week, end with `git push` to your portfolio repo.

One day a week (e.g. Saturday), spend 2 hours doing a **review**: re-read confusing parts, redo a hard lab, or read a kernel patch on lore.kernel.org just to absorb idiom.

**Don't binge.** People who study 8 hours on Sunday and 0 the rest of the week forget everything. People who study 1 hour every day remember.

---

## Three checkpoints — can you answer "yes"?

### Checkpoint 1 — End of Stage 4 (DSA)

- [ ] I can implement a singly linked list, hash table, and binary search tree from scratch in C, in 30 minutes each.
- [ ] I can compute the time complexity of a function I just wrote.
- [ ] I can solve a "medium" Leetcode problem in C without help.
- [ ] I can read kernel `list_head` / `hlist` / `rbtree` code without confusion.

If yes: continue. If no: **stay in DSA for 4 more weeks.**

### Checkpoint 2 — End of Stage 8 (OS theory)

- [ ] I can write a multi-threaded program with mutexes and condition variables that doesn't race.
- [ ] I can write a program that uses `fork`, `exec`, `pipe`, `signal` correctly.
- [ ] I can explain virtual memory, paging, and the difference between user and kernel mode.
- [ ] I can write a TCP echo server and an `epoll`-based one.

If yes: continue. If no: **redo Stages 5+7+8 with more labs.**

### Checkpoint 3 — End of Stage 11 (GPU stack)

- [ ] I have written a working kernel module with `read`/`write`/`ioctl`/`mmap`.
- [ ] I have written a PCI driver against QEMU `edu`.
- [ ] I have used libdrm_amdgpu to allocate a BO and submit a tiny PM4 IB.
- [ ] I can explain DMA, DRM scheduler, dma-fence, and GPUVM.
- [ ] I have read at least one full file in `drivers/gpu/drm/amd/amdgpu/` and understood ≥80%.

If yes: **start interviewing for FTE.** If no: pick the weakest area and re-deepen.

---

## What if I'm in a hurry?

If you have a deadline (e.g., "I have 6 months to convert from contractor to FTE"):

| Time you have | Strategy |
|---|---|
| 6 months | 2 hours/day. Skip Part X (you can read after FTE). Cut some DSA depth (do top 100 problems, not 200). Total: ~365 hours. |
| 3 months | Full-time (8 hours/day). All stages. Will be intense but doable. ~480 hours. |
| < 3 months | Stage 9–11 only, plus sufficient C. Cram the rest. Risky; reread later. |

In all cases: **don't fake mastery.** Recruiters and senior engineers can tell.

---

## What if I get stuck for a week?

Welcome to engineering. The 30-minute rule (Part 0 chapter 10): if stuck > 30 min, walk + write up your problem + search + ask. If a whole week is gone:

1. **Reduce scope**: pick the one specific concept you're stuck on; ignore the rest.
2. **Get a smaller test case**: 5 lines that exhibit your confusion.
3. **Ask in public**: amd-gfx@lists.freedesktop.org for AMD Q's, kernelnewbies.org for kernel Q's, Stack Overflow for C/Linux.
4. **Sleep on it 2 nights.** Often this alone fixes things.
5. **Go back one chapter.** Sometimes the gap is in the prerequisite.

There is no week of confusion you can't escape. Every senior engineer has had them. They keep going.

---

## The single hardest moment

Around month 4–6, you'll feel like "everyone is faster than me, I'll never get this." That moment passes. It always passes. Push through.

The 24-year PMTS engineer at AMD also had this moment. Probably more than once.

---

## Now go

Open [Part 0 / Chapter 0 — Start here](part0-zero/00-start-here.md) and begin.

Or, if you've done Stage 1 already, go to your current stage from the table above.

Welcome to the path.
