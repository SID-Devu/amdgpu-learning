# A4 — Shell power tour

You learned `pwd`, `ls`, `cd`, `cp`, `mv`, `rm` in chapter 2. The shell is much more powerful. This chapter shows you the parts you'll use every day for the rest of your career.

We're using **bash** (or zsh — they're 95% identical for this material).

## Pipes — `|`

A **pipe** sends the output of one command into the input of another. This is the magic that makes the shell composable.

```bash
ls /etc | wc -l               # how many entries in /etc?
ps aux | grep firefox         # find firefox processes
dmesg | tail -50              # last 50 kernel log lines
cat file.log | grep ERROR | wc -l   # how many ERROR lines?
```

`wc -l` counts lines. `grep pattern` keeps only lines matching. `tail -N` shows last N. `head -N` shows first N.

Read `A | B | C | D` as "do A, send result to B, send result to C, send result to D." Like a factory line.

## Redirects — `>`, `>>`, `<`, `2>`

Where output goes (or input comes from):

| Symbol | Meaning |
|---|---|
| `cmd > file` | overwrite `file` with stdout |
| `cmd >> file` | append stdout to `file` |
| `cmd < file` | use `file` as stdin |
| `cmd 2> file` | write stderr to file |
| `cmd > out 2> err` | stdout to `out`, stderr to `err` |
| `cmd > both 2>&1` | stdout AND stderr to `both` |
| `cmd &> both` | (bash shortcut) same as above |
| `cmd > /dev/null` | throw stdout away |

Examples:

```bash
make > build.log 2>&1            # capture all build output
gcc x.c 2> errors.txt            # only error messages
sort < unsorted.txt > sorted.txt # read one, write the other
```

## `grep` — find text

`grep pattern files` prints lines matching `pattern`.

```bash
grep "TODO" *.c                  # all TODOs in C files
grep -r "amdgpu_vm_init" .       # search recursively
grep -rn "EXPORT_SYMBOL" .       # -n shows line numbers
grep -ri "fence" drivers/gpu/    # -i = case insensitive
grep -v "^#" config              # invert: lines that DON'T start with #
grep -E "(foo|bar)" file         # extended regex
```

In a kernel tree, `grep -rn` is your most-used command. Modern faster alternative: **`rg` (ripgrep)** — same usage, way faster:

```bash
sudo apt install ripgrep
rg amdgpu_vm_init drivers/gpu/   # finds in seconds
```

Use `rg` whenever it's installed.

## `find` — find files

```bash
find . -name "*.c"                # all C files under current dir
find . -name "*.h" -newer foo     # files newer than foo
find . -type d -name build        # directories named "build"
find . -size +10M                 # bigger than 10 MB
find . -name "*.tmp" -delete      # delete .tmp files (CAREFUL)
find . -name "*.c" -exec wc -l {} \;   # run command on each match
```

## `xargs` — run a command per line of input

```bash
find . -name "*.c" | xargs grep -l "amdgpu"    # files containing amdgpu
echo "1 2 3 4" | xargs -n 1 echo               # one per line
ls *.txt | xargs rm                            # delete (use -f for force)
```

## Variables and `$`

```bash
NAME="Sudhanshu"
echo "Hello, $NAME"
echo 'Hello, $NAME'        # single quotes don't expand!

PATH=/usr/local/bin:$PATH  # prepend a directory to PATH
export EDITOR=vim          # export = make available to child processes
```

Useful built-ins:

```bash
$HOME       # your home directory
$USER       # your username
$PWD        # current dir
$$          # process id of the shell
$?          # exit code of last command (0 = success)
$0          # script name
$1, $2, ... # script arguments
```

## Command substitution — `$(...)`

Replace a command with its output:

```bash
echo "today is $(date)"
KERNEL_VER=$(uname -r)
echo $KERNEL_VER
DIR=/lib/modules/$(uname -r)/build
ls $DIR
```

Old syntax with backticks (still works, less readable):

```bash
echo "today is `date`"
```

Always prefer `$(...)`.

## Chaining: `&&`, `||`, `;`

```bash
make && ./run_tests             # run tests only if make succeeded
make || echo "build failed"     # message only if make failed
mkdir build; cd build           # always do both, ignoring failures
```

Rule: **`&&` for "must succeed", `||` for "or else", `;` for "just keep going."**

You'll see this *all the time*:

```bash
mkdir -p out && cd out && cmake .. && make -j8
```

If any step fails, the rest don't run. That's correct behavior for a build.

## Globs — `*`, `?`, `[]`, `{}`

```bash
ls *.c              # any C file
ls a*.c             # C files starting with a
ls ?.c              # exactly 1 character then .c
ls [abc].c          # a.c, b.c, or c.c
ls *.{c,h}          # .c or .h files (brace expansion)
ls drivers/**/*.c   # recursive (bash needs `shopt -s globstar`)
```

The shell expands these *before* the command runs. `ls *.c` becomes `ls a.c b.c main.c` ... before `ls` even sees it.

## History

```bash
history                  # show last commands
!!                       # rerun last command
!42                      # rerun command #42 from history
!grep                    # rerun last command starting with "grep"
Ctrl+R                   # interactive reverse search (super useful)
```

`Ctrl+R` then start typing a fragment of a previous command — bash searches history. Press `Ctrl+R` again to find older matches. Press Enter to run, or → to edit.

## Useful one-liners

```bash
# Most-recently-modified files
ls -lt | head

# Disk usage of current dir, sorted
du -sh * | sort -h

# Find big files
find / -size +1G 2>/dev/null

# Watch a command repeatedly
watch -n 1 "cat /sys/kernel/debug/dri/0/amdgpu_pm_info"

# Process tree
ps fauxw | less
pstree

# Memory + CPU snapshot
top                      # press q to quit
htop                     # nicer (sudo apt install htop)

# Network
ss -tulpn                # listening ports
ip a                     # interfaces
```

## Shell scripts

A shell script is just a file containing shell commands. First line says which shell:

```bash
#!/bin/bash
set -e                  # stop on first error
set -u                  # error on undefined variables

SRC=~/work/linux
BUILD=$SRC/build

cd "$SRC"
make O="$BUILD" -j$(nproc)
echo "build OK"
```

Save as `build.sh`, then:

```bash
chmod +x build.sh
./build.sh
```

`set -e` and `set -u` are critical. Without them, scripts swallow errors and you debug for hours. Always use them.

## Functions in scripts

```bash
log() {
    echo "[$(date +%H:%M:%S)] $*"
}

log "Starting build"
make
log "Build complete"
```

Functions take args as `$1`, `$2`, ... or `$@` (all of them).

## Loops

```bash
for f in *.c; do
    echo "compiling $f"
    gcc -c "$f"
done

i=0
while [ $i -lt 10 ]; do
    echo $i
    i=$((i + 1))
done
```

Note: `[ ... ]` is a command (`test`). Spaces around brackets are required. `$(( ... ))` is arithmetic.

## Real-world combos

These are patterns you'll re-use forever.

```bash
# Find every C file containing "foo", show file:line:match
rg -n "foo" --type c

# Count lines of code in a directory
find . -name "*.c" | xargs wc -l | tail -1

# Diff two directories' file lists
diff <(ls a) <(ls b)

# Run a command 100 times and capture timing
for i in {1..100}; do
    time ./benchmark
done 2>&1 | grep real

# Tail a log file as it grows
tail -f /var/log/syslog

# Rerun until success
until make; do
    echo "fail, retry"
    sleep 1
done

# Make a backup before editing
cp file.c file.c.bak
nano file.c
diff file.c.bak file.c
```

## Aliases — keep your hands fast

In `~/.bashrc`:

```bash
alias ll='ls -lah'
alias gs='git status -sb'
alias ka='sudo make modules_install && sudo make install'
alias dmw='sudo dmesg -w'
alias k='cd ~/work/linux'
```

Reload with `source ~/.bashrc`. Saves keystrokes; saves wrist tendons.

## Cheat-sheet card (print this)

```
| ; & |   chain commands
| && ||   conditional chain
| > >> <  redirects
| | (pipe) cmd1 | cmd2
| $(...)  command substitution
| $VAR    variable
| *.c     glob
| 2>&1    merge stderr to stdout
| Ctrl+R  search history
| Ctrl+C  cancel
| Ctrl+D  end of input / logout
| Ctrl+L  clear screen
| Ctrl+A / Ctrl+E  start / end of line
| Alt+B / Alt+F   back / forward word
```

## Try it

1. Count how many `.c` files are under `/usr/src/linux*/drivers/gpu/` (or any dir on your machine).
2. Find every line containing `EXPORT_SYMBOL` in your home dir's projects, with file and line number.
3. Write a one-liner to print the 5 biggest files in your home dir.
4. Write a 5-line shell script that creates a folder, cd's in, runs `git init`, and creates a README.md.
5. Find a process and kill it (without using `kill -9` first; try just `kill`).

Spend an hour playing with these. The shell is a multi-tool — every minute here saves hours later.
