# Chapter 3 — Files and paths

In chapter 2 you ran `ls` and saw filenames. Where are those files actually stored, and what does `/home/sudhdevu/Documents/` even mean? That's this chapter.

## Everything is a file

Linux has a famous saying: **"everything is a file."** It's not literally true, but it's a useful starting point.

- Documents are files.
- Photos are files.
- Programs (the things you run) are files.
- Even your terminal's connection to the keyboard is a file (`/dev/tty`).
- Even your GPU device is a file (`/dev/dri/card0`). Yes, really.

This is why Linux has so many small commands like `cat`, `ls`, `cp` — once you can manipulate files, you can manipulate almost everything.

## The filesystem is a tree

Files live in **folders** (also called **directories** — same thing). Folders can hold other folders. The whole structure is a *tree* with one root:

```
/                          ← the root, called "slash"
├── bin/                   ← essential programs
├── boot/                  ← kernel and bootloader files
├── dev/                   ← device files (your GPU lives here)
├── etc/                   ← system configuration
├── home/                  ← users' home directories
│   └── sudhdevu/          ← your home (named after your username)
│       ├── Documents/
│       ├── Downloads/
│       └── ldd-textbook/
├── lib/                   ← shared libraries
├── proc/                  ← live kernel info (not real files)
├── root/                  ← the root user's home (not the same as /)
├── sys/                   ← live device tree
├── tmp/                   ← temporary files
├── usr/                   ← installed software
│   ├── bin/
│   ├── lib/
│   └── share/
└── var/                   ← logs and changing data
```

Two things to notice:

1. **There is one root, `/`.** Unlike Windows, where each disk is `C:\`, `D:\`, etc. In Linux, every disk is mounted *under* `/`. Your USB drive might appear at `/media/usb`. Still under `/`.
2. **Slashes go forward.** `path/to/thing`, never `path\to\thing`.

## What is a "path"?

A **path** is a string that names a location in the tree. There are two kinds.

### Absolute paths

Start with `/`. Spelled out from the root:

```
/home/sudhdevu/ldd-textbook/log.txt
```

This means: from `/`, go into `home`, then `sudhdevu`, then `ldd-textbook`, find `log.txt`.

Absolute paths always work no matter where you are.

### Relative paths

Start with anything **else**. Read relative to your current directory.

If you are inside `/home/sudhdevu`, then:

```
ldd-textbook/log.txt
```

is short for `/home/sudhdevu/ldd-textbook/log.txt`. The shell fills in the front.

### `.` and `..`

- `.` (a single dot) = "the current directory" (where you are now).
- `..` (two dots) = "the parent directory" (one level up).

Examples, assuming you are in `/home/sudhdevu/ldd-textbook/`:

| You type | Means |
|---|---|
| `cat log.txt` | `/home/sudhdevu/ldd-textbook/log.txt` |
| `cat ./log.txt` | exactly the same — `.` makes it explicit |
| `cat ../Documents/x.txt` | `/home/sudhdevu/Documents/x.txt` (go up one, then into Documents) |
| `cd ..` | go to `/home/sudhdevu/` |
| `cd ../..` | go to `/home/` |
| `cd ../../..` | go to `/` |

### `~`

`~` (tilde) means "your home directory." Almost always `/home/yourname/`.

```bash
cd ~                # same as cd /home/sudhdevu
cd ~/Documents      # same as cd /home/sudhdevu/Documents
echo $HOME          # prints your home dir
```

## Practice this until you don't have to think

In your terminal, do this exact sequence and predict the output of each `pwd` *before* you press Enter:

```bash
cd /
pwd
cd home
pwd
cd ~
pwd
cd ..
pwd
cd ./../..
pwd
cd
pwd
```

(`cd` with no arguments = `cd ~`.)

Did you predict every step right? If not, draw the tree on paper and walk through it.

## Hidden files

Files whose names start with `.` are **hidden**. `ls` doesn't show them by default; `ls -a` does.

```bash
cd ~
ls
ls -a
```

You will see things like `.bashrc`, `.vimrc`, `.ssh/`. These are usually configuration files. The dot doesn't make them special, it's just a convention to keep them out of the way.

## Permissions, very briefly

When you ran `ls -l` you saw something like:

```
-rw-r--r-- 1 sudhdevu sudhdevu 30 May 7 14:00 log.txt
```

The `-rw-r--r--` is the **permissions string**. Read it as four groups:

```
-   rw-   r--   r--
type owner group others
```

- **type**: `-` means regular file, `d` means directory, `l` means symbolic link, `c`/`b` mean character/block device files.
- **owner / group / others**: who can `r`ead, `w`rite, e`x`ecute the file.

For now you mostly don't worry about this. When something says "Permission denied," you'll come back here. Or use `chmod`/`chown`, which we'll cover later.

## Useful path tricks

### Wildcards

```bash
ls *.txt          # all files ending in .txt
ls a*             # all files starting with 'a'
ls ?.txt          # exactly 1 char + .txt (a.txt yes, ab.txt no)
ls /etc/*.conf    # full paths work too
```

`*` and `?` are expanded by the shell **before** the command runs. So `ls *.txt` becomes `ls a.txt b.txt c.txt`.

### Two filenames at once with `{}`

```bash
mkdir -p {src,build,test}     # makes 3 dirs
cp file.{txt,bak}             # copy file.txt to file.bak
```

### Finding files

```bash
find ~/ldd-textbook -name "*.md"    # find all .md files under that dir
which gcc                            # which file gets run when I type 'gcc'
locate amdgpu.ko                     # fast search of a precomputed db (if installed)
```

You will use `find` often when working in the kernel tree.

## Special directories you should know exist

| Path | What's there |
|---|---|
| `/home/youruser/` | your stuff |
| `/etc/` | system configuration files |
| `/var/log/` | system logs (`/var/log/syslog`, `/var/log/dmesg`) |
| `/usr/include/` | C header files (`stdio.h`, etc.) |
| `/usr/bin/`, `/bin/` | installed programs |
| `/usr/lib/`, `/lib/` | shared libraries |
| `/proc/` | live process info (e.g., `/proc/cpuinfo` shows your CPU) |
| `/sys/` | live device tree (e.g., `/sys/class/drm/` shows your GPUs) |
| `/dev/` | device files (e.g., `/dev/dri/card0` is your GPU) |
| `/tmp/` | scratch space, cleared on reboot |

Try these now:

```bash
cat /proc/cpuinfo | head      # info about your CPU
ls /sys/class/drm             # your display devices
ls /dev/dri                   # your GPU files (if you have a GPU)
```

That `card0` you may see *is* the file your future driver code will create. You're already touching it.

## Try it (10 minutes)

1. From your home directory, navigate to `/usr/include/` using only relative paths (no absolute path, no `~`). How many `..` do you need?
2. Make this exact tree under `~/practice`:
   ```
   practice/
   ├── parts/
   │   ├── part0/
   │   └── part1/
   └── README.md
   ```
   (Hint: `mkdir -p` creates parents as needed.)
3. From inside `parts/part0/`, write a relative path that names `practice/README.md`.
4. From inside `parts/part0/`, write a relative path that names `parts/part1/`.
5. Use `find` to find every `.md` file under `~/ldd-textbook`. (If you've copied the book, there should be many.)

## Self-check

- What does `/` mean by itself? What about `/home/`?
- What's the difference between `~`, `.`, and `..`?
- If your current directory is `/a/b/c`, what is `../../../etc`? (Answer: `/etc`.)
- Why is Linux's "everything is a file" model useful for drivers?

You now know how to find anything on your computer. Next we learn how to *edit* files, since "open Notepad" doesn't apply here.

➡️ Next: [Chapter 4 — Text editors](04-text-editors.md)
