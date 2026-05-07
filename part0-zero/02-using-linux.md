# Chapter 2 — Using Linux: the terminal

The **terminal** (also called the **shell** or **command line**) is a window where you type instructions and the computer does them. It looks scary because it's all black and text. It is not scary. It is your most powerful tool.

Everyone at AMD uses a terminal every day. After this chapter, so will you.

## Open a terminal

- **On Ubuntu/Linux Mint**: press `Ctrl + Alt + T`.
- **On WSL2 (Windows)**: open the **Ubuntu** app from your Start Menu.
- **On Mac**: press `Cmd + Space`, type "Terminal", press Enter. (For full Linux compatibility, install Linux in a VM later.)

You'll see something like this:

```
sudhdevu@laptop:~$
```

That's called the **prompt**. It is waiting for you to type. Every time you see `$` in this book, that's the prompt — you don't type the `$`, you type after it.

## Your first command

Type this and press Enter:

```bash
echo "hello"
```

You should see:

```
hello
```

That's it. You used a computer the way professionals use it.

`echo` is a command that just prints whatever you give it. Try a few more:

```bash
echo "I am learning Linux"
echo 1 + 1
```

The second one prints `1 + 1`, not `2`. The terminal does not do math. `echo` just repeats words.

## What is a "command"?

A command is a tiny program that does one thing. Linux comes with hundreds of them. You will only use about 30 every day. Here are the absolute essentials. **Type each one.** Don't read them.

### See where you are

```bash
pwd
```

`pwd` = **print working directory**. The "directory" is the folder you are currently inside. It will print something like `/home/sudhdevu`. We'll explain what that means in chapter 3.

### List what's here

```bash
ls
```

`ls` = **list**. Shows the files and folders in the current directory.

Variations:

```bash
ls -l        # long form, shows sizes and dates
ls -a        # also shows "hidden" files (those starting with .)
ls -la       # both
ls /etc      # list a specific directory
```

The `-l`, `-a` parts are called **flags** or **options**. They modify what the command does.

### Move into a directory

```bash
cd Documents
```

`cd` = **change directory**. Now you are inside `Documents`. Run `pwd` and `ls` to confirm.

```bash
cd ..        # go back up one directory
cd ~         # go to your home directory
cd /         # go to the root of the filesystem
cd -         # go back to the last directory you were in
```

### Make a folder

```bash
mkdir my-first-folder
ls
cd my-first-folder
pwd
```

You just created a folder, listed your current dir to confirm, moved into it, and printed where you are.

### Make a file (the easy way)

```bash
touch hello.txt
ls
```

`touch` creates an empty file (or, if it exists, just updates its timestamp).

### See what's inside a file

```bash
echo "this is some text" > note.txt
cat note.txt
```

The `>` says "take whatever this command would print, and put it in a file instead." So `echo "this is some text" > note.txt` creates `note.txt` with the text inside.

`cat` = **concatenate**, but in practice it just dumps the contents of a file to your screen.

For long files, use `less`:

```bash
less /etc/services
```

In `less`: press `Space` to scroll down, `b` to scroll up, `/word` to search, `q` to quit.

### Copy, move, delete

```bash
cp note.txt note-copy.txt        # copy
mv note-copy.txt renamed.txt     # rename / move
rm renamed.txt                   # delete (PERMANENT — no recycle bin!)
```

**`rm` does not move things to a recycle bin. Once it's gone, it's gone.** Be careful.

The single most dangerous command in Linux is:

```bash
rm -rf /        # NEVER RUN THIS — it deletes everything
```

Don't run that. Don't even type it.

### Find help

Every command has a manual page. Try:

```bash
man ls
```

(`q` to quit.) The man pages are dense. As a beginner, prefer:

```bash
ls --help
```

It usually fits on one screen.

## A useful starter session, end to end

Type all of this. Don't skip any step.

```bash
cd ~                                  # go home
mkdir ldd-textbook                    # make a folder for this book's work
cd ldd-textbook                       # enter it
pwd                                   # confirm
echo "I started Linux Device Drivers" > log.txt
cat log.txt
ls -l
```

Output of the last line will look like:

```
-rw-r--r-- 1 sudhdevu sudhdevu 30 May  7 14:00 log.txt
```

This is a file listing. Most of those columns are explained in chapter 3 (file permissions). For now: there's one file, named `log.txt`, and it's 30 bytes.

## The 12 commands you must memorize

| Command | What it does |
|---|---|
| `pwd` | print where you are |
| `ls` | list files here |
| `cd path` | change directory |
| `mkdir name` | make folder |
| `touch name` | make empty file |
| `cp src dst` | copy |
| `mv src dst` | move / rename |
| `rm name` | delete |
| `cat file` | print file |
| `less file` | view file scrolling |
| `echo "text"` | print text |
| `clear` | clear the screen |

Add 6 more for everyday work:

| Command | What it does |
|---|---|
| `man cmd` | show manual |
| `cmd --help` | show short help |
| `which cmd` | where the command lives |
| `history` | show what you typed before |
| `Ctrl+C` | cancel a running command |
| `Ctrl+L` | same as `clear` |

That's 18 things. Master them this week.

## Tab completion (huge timesaver)

When you type the start of a name, press **Tab**. The shell completes it for you.

```bash
cd Doc<Tab>             # becomes cd Documents/
ls /etc/syst<Tab>       # becomes ls /etc/system... or shows possibilities
```

Tab is your friend. **Use it all the time.** It also prevents typos.

## Up arrow

Press the **Up Arrow** to recall the previous command. Press it again for the one before. This is how engineers re-run a build 50 times: they just hit Up + Enter.

## Try it (15 minutes)

1. Open a terminal.
2. Make a folder called `practice` in your home directory.
3. Inside it, make 5 files: `a.txt`, `b.txt`, `c.txt`, `d.txt`, `e.txt`. Each should contain its own letter (use `echo "a" > a.txt`, etc.).
4. List them long form (`ls -l`).
5. Make a folder called `letters` and move all of them into it.
6. Inside `letters`, copy `a.txt` to `aa.txt`.
7. Print all of them concatenated to your screen using `cat *.txt` (the `*` means "all matching files").
8. Delete `aa.txt`.
9. Go up one level and delete the whole `practice` folder.
   - Hint: `rm -r practice` (the `-r` means "recursive — go into directories").

If you made it through, you have all the Linux skills you need to get started.

## What if I get stuck?

You will. That's chapter 10. For now, the rule is:

- If a command says "command not found," you typed it wrong.
- If a command says "Permission denied," you don't have rights to that file (don't worry, normal in `/etc`).
- If you see a screen you don't recognize and don't know how to leave, try **q**, then **Esc**, then **Ctrl+C**, then close the terminal and reopen.

## Self-check

- What does `pwd` mean and why would you use it?
- What's the difference between `cd ..` and `cd ~`?
- What's the difference between `rm` and a recycle bin?
- Why is Tab completion important?

You now control Linux from the keyboard. Tomorrow we'll talk about *where* files actually live.

➡️ Next: [Chapter 3 — Files and paths](03-files-and-paths.md)
