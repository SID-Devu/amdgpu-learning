# Chapter 4 — Text editors

To write code you have to edit text files. There are many editors. Beginners get into religious wars over them. **Don't.** Use whatever lets you make progress today, and learn `vim` later because the kernel community uses it.

## The three you need to know

| Editor | When to use |
|---|---|
| **`nano`** | Right now, today, while you're new. Easiest. |
| **VS Code** (or "Cursor", same idea) | When you write more than 50 lines at a time. |
| **`vim`** | Eventually. Mandatory if you want to be quick over SSH and read kernel patches comfortably. |

You don't have to pick. You'll use all three over time.

## `nano` — the friendly one

`nano` is the easiest editor on Earth. To open a file:

```bash
nano hello.txt
```

You see a screen with the file contents at the top and a strip of help at the bottom:

```
GNU nano 6.2                 hello.txt

Hello, this is my first file.
_

^G Help    ^O Write Out   ^X Exit
```

Read the bottom strip. **`^X` means `Ctrl+X`.** Just type your text, then:

- `Ctrl+O` then Enter to **save** ("write out").
- `Ctrl+X` to **quit**.
- `Ctrl+K` to cut a line.
- `Ctrl+U` to paste.
- `Ctrl+W` to search.

That's enough to write and edit any file. Use `nano` for the next few chapters until something annoys you about it.

### Try it

```bash
cd ~
nano my-name.txt
# Type: My name is <your name>.
# Ctrl+O, Enter, Ctrl+X
cat my-name.txt
```

## VS Code (or Cursor)

For long code, you want a real editor with autocomplete, syntax colors, and a file tree.

- **VS Code**: free, install from your distribution's package manager or from <https://code.visualstudio.com/>.
- **Cursor**: VS Code with AI built in. Same UI.

To open a folder in VS Code from the terminal:

```bash
cd ~/ldd-textbook
code .          # open the current directory
```

You'll get a panel on the left showing all the files, and a tabbed editor on the right.

You don't need to learn 100 shortcuts now. Just know:

- `Ctrl+S` save
- `Ctrl+P` quick-open a file by name (huge time saver)
- `Ctrl+\`` (backtick) to open a terminal *inside* VS Code
- `Ctrl+/` to comment/uncomment a line

Use VS Code for **all the C and Python work** in this book. Use `nano` over SSH or for quick edits.

## `vim` — eventually

`vim` is hostile to beginners on purpose. It's also extremely fast once you know it, and the kernel community lives in it. You should learn it within the first 6 months. Today is not that day.

If you accidentally end up in `vim`:

- Press **`Esc`** (this exits any sub-mode).
- Type `:q!` and press Enter.
- That's "quit without saving." You're back at the shell.

If you have 30 spare minutes, run:

```bash
vimtutor
```

It's a built-in interactive tutorial. Do it once. Then don't worry about `vim` again until chapter 28 or so.

### vim cheat sheet (for when you're ready)

```
Modes
  Esc          go to "normal" mode (this is where you navigate)
  i            switch to "insert" mode (where you type)

Movement (normal mode)
  h j k l      left, down, up, right
  w            next word
  b            previous word
  0            start of line
  $            end of line
  gg           top of file
  G            bottom of file
  /pattern     search forward (then n / N for next / prev)

Edit
  i            insert before cursor
  a            insert after cursor
  o            open new line below
  x            delete one char
  dd           delete a line
  yy           copy ("yank") a line
  p            paste below
  u            undo
  Ctrl+R       redo

Save / quit
  :w           save
  :q           quit
  :wq          save + quit
  :q!          quit without saving
```

Print this. Tape it next to your monitor. In 2 weeks you won't need it.

## Why text editors matter

You will spend more hours in your editor than in any other program. Pick one, get fast at it. **Speed of editing is real productivity** — every minute you save fumbling with the cursor is a minute you spend thinking about the problem.

But this is a slow improvement, not an urgent one. Don't optimize before you have a workflow. **Today: open `nano`, write text, save, quit.** That's it.

## A common editor mistake

If you `nano file.c` then later run a command and edit the same file in VS Code, you might end up with two views of the file open. If you save in one and then save in the other, the second save **overwrites** the first.

Rule: edit a file in **one place at a time**. Save before switching.

## Try it (10 minutes)

1. Use `nano` to create `~/ldd-textbook/notes.md`. Write 3 sentences about chapter 3. Save and quit.
2. Print it with `cat`.
3. Install VS Code if you haven't (`sudo apt install code` may work, otherwise download from the website).
4. Open `~/ldd-textbook/` in VS Code.
5. Open `notes.md` in VS Code, add a 4th sentence, save (`Ctrl+S`).
6. Back in the terminal, `cat notes.md` again. Confirm the new sentence is there.

## Self-check

- How do you save and quit `nano`?
- How do you save and quit `vim`? (Trick: include "what if you didn't save and want to abandon changes?")
- Why might you prefer VS Code for big projects but `nano` over SSH?

You can now edit any file. Now we get to the actual point of this book — writing code.

➡️ Next: [Chapter 5 — What is code?](05-what-is-code.md)
