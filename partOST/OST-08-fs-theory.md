# OST 8 — File system theory & journaling

> A filesystem turns "blocks of bytes on a disk" into "files in a tree." The data structures and crash-safety stories are gold.

## What a filesystem must provide

- **Naming**: turn names into files (path resolution).
- **Storage**: efficient layout on disk.
- **Metadata**: size, owner, permissions, timestamps.
- **Concurrency**: many processes, one FS.
- **Durability**: survive crashes.
- **Performance**: reads, writes, sync.

These goals interact. Real FSes pick trade-offs.

## On-disk layout (classic ext-style)

A disk is divided into blocks (4 KB typical):

```
+----------------+--------------------+----------------+--------------------+
| Boot block(s)  |  Superblock        | inode table    | data blocks        |
+----------------+--------------------+----------------+--------------------+
```

- **Superblock**: FS metadata (block size, total blocks, FS UUID, journal location).
- **Inode**: metadata for one file (size, owner, perms, list of data blocks). NO filename.
- **Data block**: actual file contents.
- **Directory**: a file whose contents are name → inode pairs.

Names live in directories, not in inodes. **Hard linking** (multiple names → same inode) is therefore natural.

## Inode

Typical fields:
- mode (file type + permissions).
- uid, gid.
- size.
- atime, mtime, ctime.
- direct block pointers (e.g., 12).
- single, double, triple indirect block pointers.

ext4 uses **extents** (start + length) instead of block pointers — far more efficient for large contiguous files.

## Path resolution

`/usr/local/bin/foo` → start from root inode → look up "usr" in dir entries → its inode → look up "local" → ... → final inode.

Each lookup involves potentially:
- Reading the directory file.
- Reading the inode.
- Permission checks.
- Symlink following.

Linux's **dentry cache** caches resolved paths. Critical for performance.

## Allocation strategies

How does the FS pick a block for a new write?

- **Bitmap free list**: one bit per block. Find first zero.
- **Buddy-like**: bigger free regions tracked separately.
- **Extents**: track contiguous free regions.

**Locality** matters — placing a file's blocks close together reduces seek time on rotational disks. SSDs care less but cache lines / erase blocks still exist.

## Concurrency in FS

Multiple processes modifying same file or same directory → race. Handled with:
- per-inode locks,
- per-directory locks,
- journaling for crash atomicity.

## Crash consistency — the hard part

A crash mid-write can leave the FS in an inconsistent state:
- Inode says the file is 10 KB, but only 5 KB of data is on disk.
- Directory has entry for inode that doesn't exist.
- Free list says block free, but it's actually in use.

Approaches:

### `fsck` — scan and repair
After crash, scan everything; fix inconsistencies. Slow (minutes to hours on big disks). Old-style.

### Journaling — write-ahead log
Before modifying real structures, write the **intent** to a journal. After crash, replay journal.

Two flavors:
- **Data journaling**: log both metadata and data. Safe; double-writes (slow).
- **Metadata journaling**: only metadata in journal; data flushed before. Faster; data ordering invariant: data on disk before metadata refers to it.

ext3/ext4 default: metadata journal with `data=ordered` (data before metadata). xfs similar.

### Soft updates (FreeBSD's UFS2)
Carefully order writes so the FS is always consistent. No journal. Complex to implement. Mostly historical.

### Copy-on-write (ZFS, btrfs)
Never overwrite. Write new blocks; update pointers atomically (sometimes a single sector write at the root). Eternal consistency. Snapshots are free. Cost: more I/O, more fragmentation; needs garbage collection.

### Log-structured (LFS, F2FS for SSDs)
Treat the disk as a log: append everything; periodically clean (compact) the log. Great for SSDs (avoids in-place writes that wear out flash).

## fsync & friends — durability boundaries

`write` puts data in the page cache. `fsync(fd)` waits until data is on disk. Without `fsync`, a crash can lose recent writes.

Some disks lie about being persistent (power-loss-protection-less SSDs). `O_SYNC` and barriers ensure ordering, but you trust the device.

## File system zoo

| FS | Notes |
|---|---|
| ext4 | Linux default for years. Extent-based, journaled. Mature. |
| xfs | Originally SGI; great for large files / high concurrency. |
| btrfs | Copy-on-write; snapshots, RAID, checksums. Mature; some users still prefer ZFS. |
| ZFS | OpenZFS on Linux. CoW + checksums + RAID-Z + ARC. Heavy. |
| F2FS | Flash-friendly, log-structured. Used in some Android devices. |
| tmpfs | RAM-backed; POSIX shared memory uses it. |
| FAT32 / exFAT | USB drives, SD cards. Simple, no journaling. |
| NTFS | Windows. Linux can read/write via ntfs3 driver. |
| iso9660 | CDs. Read-only. |
| nfs / cifs | Network file systems. |
| FUSE | "Filesystem in Userspace"; lets you write FSes as regular processes. SSHFS, gocryptfs, etc. |

## VFS — Virtual Filesystem Switch

Linux's abstraction: an in-kernel API that all filesystems implement. Userspace `read("/somefile")` flows through:

```
syscall: sys_read
        -> vfs_read
        -> file->f_op->read_iter   (depends on FS)
        -> ext4_read_iter (e.g.)
        -> page cache or disk
```

VFS objects: **superblock** (FS instance), **inode**, **dentry** (cached path entry), **file** (open file).

We'll cover VFS in Part V.

## Network filesystems

NFS: simple RPC; client caches inodes/dentries. Coherence is "loose" (close-to-open). Used in HPC clusters.

CIFS/SMB: Windows file sharing.

Object stores (S3, Ceph): not POSIX — different API.

Modern distributed: Lustre, GlusterFS, CephFS for parallel HPC.

## Performance issues

- **fsync latency**: SSDs ~1ms, HDDs 10ms+. Critical for DB commit.
- **Metadata-heavy** (lots of small files): inode + dentry cache misses dominate.
- **Fragmentation**: ext4/xfs handle well; severe on FAT.
- **Compression / encryption**: can speed up (less I/O) or slow down (CPU bound).
- **RAID**: parity calc costs writes; raid-1 doubles writes, raid-5/6 has read-modify-write penalty.

## Try it

1. `df -hT` — what FS is each mount? Identify ext4, tmpfs, etc.
2. Make a 100 MB file with `dd`. `fdisk -l` (or `lsblk`); `tune2fs -l /dev/...` to inspect ext4 metadata.
3. Run a benchmark: `dd if=/dev/zero of=test bs=1M count=1000 oflag=direct` vs without `oflag=direct`.
4. Use `iotop` while doing massive writes; observe.
5. Read OSTEP chapters 39–43 (file systems).
6. Bonus: write a tiny FUSE filesystem (e.g., a "memory FS" that lives in RAM only).

## Read deeper

- OSTEP chapters 39–45 — entire FS section.
- *The Linux Programming Interface* chapters on FS.
- For btrfs: "An introduction to btrfs" — multiple LWN articles.
- For ZFS: "End-to-End Data Integrity for File Systems" (Bonwick & Moore).

Next → [Crash consistency & write ordering](OST-09-crash-consistency.md).
