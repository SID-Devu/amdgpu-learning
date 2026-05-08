# OST 9 — Crash consistency & write ordering

> Power dies. RAM goes. Disk has half-old, half-new state. How do real systems survive that?

## The problem

You issue these writes:
```
1. write inode (file size = 10K)
2. write data block #1
3. write data block #2
```

Crash anywhere → state is partially applied. Possible bad outcomes:
- (1) without (2,3) → inode says 10K, file is junk after the first 0 bytes.
- (2,3) without (1) → data on disk but no inode pointing to it = leak.
- (3) without (1,2) → orphan data block.

You need either **all-or-nothing** or **a known correct intermediate state**.

## Tools the OS uses

### Atomic single-sector write
Most disks guarantee a single sector (512 B or 4 KB) write is atomic — either fully succeeds or doesn't. Bigger writes can be torn.

### Barriers / ordering
Tell the disk "don't reorder this past that point." Implemented via FUA (Force Unit Access) flags, cache flush commands.

The kernel's block layer enforces these for FSes that ask. When you do `fsync`, the FS issues a flush.

### Checkpoints
Periodically declare the disk consistent. After crash, recover from the latest checkpoint forward.

## Approaches

### 1. fsck-after-crash (very old)

Run `fsck` after every crash. Slow but simple. ext2 worked this way.

### 2. Write-ahead log (WAL) / journal

Before modifying actual data, write a record of what you're about to do.

Sequence:
1. Write the journal entry. Issue a barrier.
2. Apply the changes to FS metadata + data.
3. Mark the journal entry as committed. (Or just rely on transaction order.)
4. Periodically truncate the journal.

After crash:
- Replay all uncommitted journal entries to redo (or to undo, depending on protocol).

ext3/ext4, NTFS, XFS, ZFS (zil), most SQL DBs.

Modes (ext4):
- `data=journal`: journal everything, including data. Safe. Slow.
- `data=ordered` (default): journal metadata; **flush data before metadata** that points to it. Common balance.
- `data=writeback`: journal metadata only; **no data ordering**. Fastest, files can have stale data after crash.

### 3. Soft updates

Order writes carefully so the FS is **always** consistent on-disk. E.g., for "create file":
1. Write inode (with refcount=0).
2. Write directory entry.
3. Write inode (refcount=1).

If crash between 1 and 2 → orphan inode (recoverable via fsck pass).

Implementations have **dependency tracking** in memory; complex.

### 4. Copy-on-write (CoW)

Never overwrite. Build new state in new blocks; update root pointer atomically (one sector). All previous state remains intact.

ZFS, btrfs.

Pros: trivial consistency. Snapshots are free (just keep old root). Checksums can verify.
Cons: more writes (every change = new blocks all the way to root). Garbage collection. Fragmentation.

### 5. Log-structured

Treat the disk like a log; append everything. Old data left in place; cleaner / GC reclaims periodically.

Excellent for SSDs (only forward writes). LFS (1992), F2FS (Linux), modern flash FSes.

## Atomic rename — the user's friend

POSIX guarantees `rename(old, new)` is atomic on the same filesystem (within one inode/path). So safe-write-and-replace pattern:

```
1. write data to "foo.tmp"
2. fsync("foo.tmp")
3. rename("foo.tmp", "foo")     // atomic
```

Now `foo` is either fully old or fully new — never half-written.

This is **the most-used safe-write pattern**. Editors use it; package managers; everywhere.

## Disk surprises you should know

- Some disks have **volatile write caches**: they ack the write before it's actually durable. Cache flushes are slow but necessary.
- **Power loss during firmware-internal operation** can corrupt disks (rare; mostly cheap drives).
- **Bit rot**: data can decay over time. Filesystems with checksums (ZFS, btrfs) detect; others silently corrupt.
- **SSDs**: write amplification, garbage collection, wear leveling. Operating systems issue **TRIM** to inform the SSD which blocks are free.

## Real-world durability (databases)

PostgreSQL, MySQL, etc. write a **WAL** (write-ahead log). Commit = write WAL + fsync. Asynchronous flush of data pages.

After crash:
1. Read WAL.
2. Re-apply committed transactions.
3. Roll back uncommitted ones.

Same idea as FS journal, deeper structure. **Never lose committed data.**

`fsync` is the **transaction commit's** durability boundary.

## Common pitfalls

1. **`write()` without `fsync()` ≠ durable.**
2. **Renaming on a different filesystem** silently degrades to copy + delete. Atomic rename only **within** an FS.
3. **Renaming a file pointed to by an open fd** — the fd still works, fine.
4. **Caches lie**: enterprise SSDs have power-loss capacitors; consumer SSDs may not.
5. **fsync on a directory**: needed if you want the directory entry change (e.g., new file) durable.
6. **NFS**: `fsync` over NFS has historical bugs; check the man page for your distro.

## Try it

1. Implement "safe save": write to `tmp`, `fsync`, `rename`. Test by SIGKILLing the writer; verify file is either old or new, never half.
2. Stress-test crash safety: in a VM, periodically fsync after every commit, then reboot the VM hard. Verify your data is intact.
3. Read the ext4 journal log: `tune2fs -l /dev/...` shows journal device and inode. Use `debugfs` to inspect.
4. Read OSTEP chapter 42 (FSCK and Journaling).
5. Read the `dwarfs` or `bcachefs` design docs (modern Linux FSes).

## Read deeper

- "Crash Consistency: FSCK and Journaling" — OSTEP chapter 42.
- "All File Systems Are Not Created Equal" by Pillai et al. (OSDI 2014) — devastating analysis of fsync usage.
- "Files are hard" — Dan Luu's blog post.
- Postgres's "Write-Ahead Logging" docs.

Next → [Distributed systems intro](OST-10-distributed-intro.md).
