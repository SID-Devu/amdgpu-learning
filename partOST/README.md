# Part OST — Classic OS Theory (the academic foundation)

> **Stage 8 of the [LEARNING PATH](../LEARNING-PATH.md). 4–6 weeks at 1 hr/day, in parallel with Part III.**

You've done multi-threading and userspace systems. Now: **the academic theory that underlies every OS.**

This part complements [Part III — OS Internals](../part3-os/) (which is Linux-specific). Part III tells you *how Linux works*; Part OST tells you *why it works that way and what alternatives exist.* Together they make you OS-fluent.

## Why this part exists

If you walk into a kernel interview and say "I know Linux," good. If you can also discuss:

- "Why does CFS use vruntime instead of priority?"
- "What's the trade-off between MLFQ and lottery scheduling?"
- "When does LRU lose to ARC?"
- "What's the difference between strict 2PL and snapshot isolation?"

…then you sound like a senior engineer. **That's the goal of this part.**

## The classic OS topics

Most OS textbooks (Tanenbaum, Silberschatz, OSTEP, Stallings) cover the same eternal topics:

1. Process model & threading.
2. Scheduling.
3. Synchronization (semaphores, monitors, lock-free).
4. Deadlock.
5. Memory management (paging, segmentation, swapping).
6. File systems & journaling.
7. I/O subsystem.
8. Distributed / concurrency basics.
9. Security & isolation.
10. Virtualization.

We do them at the right level for a kernel engineer (not too academic, not too cookbook).

## Table of contents

| # | Chapter |
|---|---|
| 1 | [Process model — formal view](OST-01-process-model.md) |
| 2 | [CPU scheduling theory](OST-02-scheduling-theory.md) |
| 3 | [Synchronization theory — Peterson, semaphores, monitors](OST-03-sync-theory.md) |
| 4 | [Classic synchronization problems](OST-04-classic-problems.md) |
| 5 | [Deadlock — prevention, avoidance, detection](OST-05-deadlock.md) |
| 6 | [Memory theory — paging, segmentation, swapping](OST-06-memory-theory.md) |
| 7 | [Page replacement algorithms](OST-07-page-replacement.md) |
| 8 | [File system theory & journaling](OST-08-fs-theory.md) |
| 9 | [Crash consistency & write ordering](OST-09-crash-consistency.md) |
| 10 | [Distributed systems intro — consensus, RPC, time](OST-10-distributed-intro.md) |

## Recommended companion text

- **OSTEP** (Operating Systems: Three Easy Pieces) by Remzi & Andrea Arpaci-Dusseau — **free PDF online**, beautifully written. The single best modern OS textbook. Read it alongside this part.
- *Modern Operating Systems* by Tanenbaum — encyclopedic.
- *Operating Systems: Internals and Design Principles* by Stallings — heavy on theory.

This Part OST is your **summary + Linux mapping**; OSTEP is your deeper read.

## How this part fits

By the time you finish:

- You know **CFS** is one approach (Linux's). You know about MLFQ, lottery, deadline, EDF — and you can argue why CFS chose what it did.
- You know **Linux uses LRU** in the page cache (with twist). You know about ARC, 2Q, LFU; the trade-offs.
- You know **ext4 journals**. You know there are crash-consistency techniques (soft updates, COW like btrfs, WAL like databases).
- You know **TCP** is a reliable transport. You know the CAP theorem and why distributed consensus is hard.

This is what separates a "competent junior" from a "thinks at the level of architecture."

→ [OST-01 — Process model: formal view](OST-01-process-model.md).
