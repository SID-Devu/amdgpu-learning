# A5 — Character encoding: ASCII, UTF-8, and why text is harder than it looks

A computer doesn't store letters. It stores bytes. To turn bytes into letters you need an **encoding** — a table that says "byte 0x41 means 'A'."

You will hit encoding bugs when:

- a `printf` in your driver outputs garbage,
- a file copied from Windows shows weird characters,
- a regex doesn't match what you expect,
- a shader compiler chokes on a Unicode quote that snuck in.

A short chapter, a useful chapter.

## ASCII — the original 128 characters

ASCII (American Standard Code for Information Interchange, 1963) maps 128 codes (0–127) to characters. Every modern encoding starts with these.

The important rows:

```
Code  Char       Notes
----  ----       ----
0x00  NUL        end of string in C ("\0")
0x09  TAB        \t
0x0A  LF         newline (Linux), \n
0x0D  CR         carriage return, \r (Windows uses CR+LF)
0x20  SPACE
0x21  !
0x22  "
0x27  '
0x2F  /
0x30..0x39  0..9
0x41..0x5A  A..Z
0x5C  \
0x61..0x7A  a..z
0x7B  {
0x7D  }
0x7F  DEL
```

Things that fall out:

- `'A' = 0x41`, `'a' = 0x61`. Lowercase = uppercase + 0x20. So `c | 0x20` lowercases an ASCII letter; `c & ~0x20` uppercases it.
- `'0' = 0x30`. So `c - '0'` converts the character `'7'` to the integer 7. (Used in every parser.)

7 bits suffice for ASCII (128 codes); the eighth bit was originally always 0. That spare bit became "extended ASCII" in the 80s — different countries shoved their own characters into bytes 128..255. **Disaster.** Hundreds of incompatible encodings.

## Unicode — one number per character (forever)

Unicode is a giant table assigning a unique number (called a **code point**) to every character in every script in the world (and emoji).

```
U+0041   LATIN CAPITAL LETTER A         (= 0x41, same as ASCII for the first 128)
U+00E9   é (e with acute)
U+4E2D   中
U+1F600  😀
```

Code points go up to ~1.1 million. So you can't fit one in a byte. You need an **encoding** — how to turn a code point into bytes.

## UTF-8 — the only encoding you should use

UTF-8 is the dominant encoding everywhere except some Windows internals. Use it. Always.

Rules:

- ASCII code points (0..127) → one byte, identical to ASCII. **UTF-8 is a strict superset of ASCII.**
- Code points 128..2047 → two bytes.
- Code points 2048..65535 → three bytes.
- Code points 65536..1.1M → four bytes.

The bit patterns:

```
Code points         UTF-8 bytes (binary)
0x0000 .. 0x007F    0xxxxxxx
0x0080 .. 0x07FF    110xxxxx 10xxxxxx
0x0800 .. 0xFFFF    1110xxxx 10xxxxxx 10xxxxxx
0x10000 .. 0x10FFFF 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
```

Properties:

- The first byte's high bits tell you how long the sequence is.
- Continuation bytes always start with `10`.
- ASCII files are valid UTF-8 byte for byte.
- You can find character boundaries by skipping any byte starting with `10`.

So `é` (U+00E9) is `0xC3 0xA9` in UTF-8 — two bytes. If software treats it as Latin-1 (a one-byte-per-char encoding), it shows as "Ã©". This is the famous **mojibake**.

## UTF-16 (heads up)

UTF-16 uses 2-byte code units. Used by Windows internally, by Java, by JavaScript strings. Variable-length when characters need 4 bytes (surrogate pairs). **Avoid in Linux/kernel work.** Just be aware it exists when you parse Windows files.

## C strings and Unicode

`char *` in C is a *byte* string, NUL-terminated. UTF-8 fits perfectly here — `strlen` returns byte count (not character count!), but otherwise byte routines mostly work.

```c
char s[] = "café";   // in UTF-8: 'c' 'a' 'f' 0xC3 0xA9 0x00
strlen(s)            // 5 — bytes, not characters!
```

If you need actual character counts, you need a Unicode-aware library (ICU, glib, etc.). For driver work you almost never deal with non-ASCII text.

`wchar_t` and `wchar*` are an old C idea (wide chars) — avoid them in new code.

## Byte-order mark (BOM)

Some Windows tools prepend `0xEF 0xBB 0xBF` (the UTF-8 BOM) to text files. Linux tools mostly ignore it; some are confused. If a script fails with "syntax error" on the very first line, check for a BOM:

```bash
hexdump -C -n 4 myscript.sh
# 00000000  ef bb bf 23 ...   ← BOM, then '#'
```

Strip with:

```bash
sed -i '1s/^\xef\xbb\xbf//' myscript.sh
```

## Newlines: LF vs CRLF

Linux uses `\n` (one byte: `0x0A`). Windows uses `\r\n` (two bytes: `0x0D 0x0A`). Mac classic used `\r`.

Symptoms of mixed line endings:

- Bash script fails with `^M: command not found` or `bad interpreter: ... ^M`.
- Text file shows `^M` at end of lines in `vim`.
- `git diff` shows whole-file changes.

Fix:

```bash
dos2unix file.sh                 # convert CRLF → LF
sed -i 's/\r$//' file.sh         # same, no install needed
```

In `git`, set `core.autocrlf=input` (Linux) or use `.gitattributes` with `* text=auto eol=lf` to enforce.

## Locales — the `LANG` and `LC_*` variables

The shell, libc, and most tools look at environment variables to decide:

- which language messages should be in,
- how dates are formatted,
- how to sort strings (collation),
- which encoding to assume for stdin/stdout.

```bash
echo $LANG          # e.g., en_US.UTF-8
locale              # show all LC_* settings
LC_ALL=C make       # force "C" locale — pure ASCII, English errors, fast sort
```

Always set `LC_ALL=C` in scripts that parse output. Otherwise a French developer's `ls -l` may say "20 oct" instead of "20 Oct" and your parser breaks.

## In the kernel

Kernel code is plain ASCII almost always. The only places encoding matters:

- file system code (paths can be UTF-8 or whatever userspace passes),
- EDID parsing (display info, sometimes UCS-2),
- Bluetooth / network protocols with text fields.

You generally don't need to think about Unicode in driver code. Just don't paste a Unicode quote (`"` `"`) into a `printk` by accident — your editor's autocorrect can do this.

## Try it

1. What's `'A' - '0'` in C? (Answer: 0x11 = 17. Useful in hex-parsing.)
2. What's `0xC3 0xA9` as UTF-8? (Answer: `é`.)
3. Open a file with `hexdump -C file.txt | head` — find the bytes for one character of your choice.
4. Make a 1-line shell script that fails because of a CRLF line ending. Fix it with `dos2unix`.
5. Why does `LC_ALL=C sort` differ from `sort` on a list with accented characters?

You can now stop being confused about "encoding errors." Back to whichever chapter you came from.
