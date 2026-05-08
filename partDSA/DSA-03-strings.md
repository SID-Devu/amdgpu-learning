# DSA 3 — Strings (read-to-learn version)

> A C string is "an array of characters that ends with a special byte." That's the secret.

## The big idea

In C, a **string** is just an array of `char` with one rule: **the last character is `'\0'`** (the null terminator — a byte whose value is zero).

Picture the string `"hi"` in memory:

```
        +----+----+----+
chars:  | h  | i  |\0  |
        +----+----+----+
addr:    1000 1001 1002
```

That's **3 bytes**, not 2! The `\0` is real, takes one byte, and tells `printf`, `strlen`, and friends "the string ends here."

In ASCII, `'h' = 104`, `'i' = 105`, `'\0' = 0`. So in raw bytes:

```
        +-----+-----+-----+
        | 104 | 105 |  0  |
        +-----+-----+-----+
```

`printf("%s", s)` prints characters starting at `s` until it hits a `0` byte.

## Two ways to write strings in C

### Way 1: read-only string literal

```c
char *p = "hello";
```

The text `"hello"` is in **read-only memory** (the program's `.rodata` section). `p` just points to it.

```
                       (read-only memory)
              ┌──────────────────────────┐
              ↓
            +---+---+---+---+---+---+
p ──▶       | h | e | l | l | o |\0 |
            +---+---+---+---+---+---+
```

You **cannot** modify `p[0] = 'H'` — that's writing to read-only memory. Often a SIGSEGV crash.

### Way 2: writable array

```c
char a[] = "hello";
```

This **copies** the literal into an array on the stack. Now you have your own private copy:

```
                       (stack — writable)
            +---+---+---+---+---+---+
a ──▶       | h | e | l | l | o |\0 |
            +---+---+---+---+---+---+
```

`a[0] = 'H';` works. Result: `"Hello"`.

**Rule of thumb:** if you intend to modify, use `char arr[]`, not `char *p`.

## strlen — how long is the string?

```c
size_t my_strlen(const char *s) {
    const char *p = s;
    while (*p) p++;       // walk until we hit '\0'
    return p - s;         // bytes walked
}
```

`while (*p)` reads as: "while the byte pointed to by `p` is non-zero." A `\0` byte is zero, so the loop ends there.

### Trace on `"hi"` (s points to address 1000)

| Step | p (addr) | *p (the char) | *p non-zero? | action |
|---|---|---|---|---|
| start | 1000 | 'h' (104) | yes | p++ |
| | 1001 | 'i' (105) | yes | p++ |
| | 1002 | '\0' (0) | **no** | exit loop |

After loop: `p = 1002`, `s = 1000`. Return `p - s = 2`. **Length is 2** (excluding the null terminator). ✓

**Cost:** O(n). `strlen` always **scans the whole string.** Calling `strlen` inside a loop over a string is a classic performance bug — O(n) inside O(n) = O(n²).

## strcmp — compare two strings

```c
int my_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}
```

Walk through both strings together. Stop when either:
- `*a` is `'\0'` (a is done), or
- `*a != *b` (mismatch).

Return the difference of the bytes where they differed (or 0 if equal).

### Trace: compare `"cat"` and `"car"`

| Step | a points to | b points to | match? | action |
|---|---|---|---|---|
| start | 'c' | 'c' | yes | both++ |
| | 'a' | 'a' | yes | both++ |
| | **'t'** | **'r'** | NO | exit loop |

Return `(int)'t' - (int)'r' = 116 - 114 = 2`. **Positive → "cat" > "car"** lexicographically.

### Trace: compare `"abc"` and `"abc"`

| Step | a points to | b points to | match? | action |
|---|---|---|---|---|
| start | 'a' | 'a' | yes | both++ |
| | 'b' | 'b' | yes | both++ |
| | 'c' | 'c' | yes | both++ |
| | **'\0'** | '\0' | yes (but `*a` is 0) | exit loop |

Return `0 - 0 = 0`. **Equal.**

The convention: `strcmp` returns **0** for equal, **negative** if `a < b`, **positive** if `a > b`.

## The dangerous library functions (and the safe ones)

### `strcpy` — DANGEROUS

```c
char *strcpy(char *dst, const char *src);
```

Copies `src` into `dst` until `\0`. **No check** that `dst` is big enough. Classic buffer overflow:

```c
char dst[5];
strcpy(dst, "hello world");      // "hello world" is 12 bytes
```

Picture the disaster:

```
Stack:
       +---+---+---+---+---+
dst:   | h | e | l | l | o |     ← 5 bytes; out of room
       +---+---+---+---+---+
        | w | o | r | l | d | \0 |  ← writes past end into the next variable!
        +---+---+---+---+---+----+
        ↑ corrupts whatever was here
```

This is how decades of security holes were created. **Avoid `strcpy`.**

### `snprintf` — SAFE

```c
char dst[5];
snprintf(dst, sizeof(dst), "%s", "hello world");
```

`snprintf` won't write more than `sizeof(dst)` bytes (and always null-terminates inside that limit). Result: `dst = "hell"` (truncated to 4 chars + `\0`).

Truncation may not be **right**, but it's not a security hole. **Use `snprintf` for every "build a string" task.**

### `strscpy` (Linux kernel)

The kernel's preferred copy. Always null-terminates; returns the length copied (or `-E2BIG` on truncation). Read `lib/string.c` once you finish Part V.

## Common string algorithms (traced)

### Reverse a string in place

```c
void reverse_str(char *s) {
    size_t n = strlen(s);
    for (size_t i = 0, j = n - 1; i < j; i++, j--) {
        char t = s[i];
        s[i] = s[j];
        s[j] = t;
    }
}
```

Same two-pointer pattern as the array reverse from chapter 2.

#### Trace on `"hello"` (n=5)

```
Before: [h][e][l][l][o]
         ↑           ↑
         i=0         j=4
```

| Step | i | j | swap | array after |
|---|---|---|---|---|
| 1 | 0 | 4 | h ↔ o | [o][e][l][l][h] |
| 2 | 1 | 3 | e ↔ l | [o][l][l][e][h] |
| 3 | 2 | 2 | i<j false → stop | (no change) |

**Result: "olleh"** ✓

### Check palindrome

```c
int is_palindrome(const char *s) {
    size_t n = strlen(s);
    for (size_t i = 0, j = n - 1; i < j; i++, j--)
        if (s[i] != s[j]) return 0;
    return 1;
}
```

Same two pointers. Instead of swapping, **check if mirror chars match**.

#### Trace on `"racecar"` (n=7)

| Step | i | j | s[i] | s[j] | match? |
|---|---|---|---|---|---|
| 1 | 0 | 6 | 'r' | 'r' | yes |
| 2 | 1 | 5 | 'a' | 'a' | yes |
| 3 | 2 | 4 | 'c' | 'c' | yes |
| 4 | 3 | 3 | i<j false → stop |   |   |

Return `1` (palindrome). ✓

#### Trace on `"hello"` (n=5)

| Step | i | j | s[i] | s[j] | match? |
|---|---|---|---|---|---|
| 1 | 0 | 4 | 'h' | 'o' | **NO** → return 0 |

Not a palindrome. ✓

### Anagram check (count of each char)

Two strings are anagrams if they have the same letters with the same counts (any order). E.g., `"listen"` and `"silent"`.

The trick: a **frequency table** of size 256 (one slot per possible byte). Increment for the first string, decrement for the second. If all counts end at zero, they're anagrams.

```c
int is_anagram(const char *a, const char *b) {
    int cnt[256] = {0};
    while (*a) cnt[(unsigned char)*a++]++;
    while (*b) cnt[(unsigned char)*b++]--;
    for (int i = 0; i < 256; i++)
        if (cnt[i] != 0) return 0;
    return 1;
}
```

#### Trace on `"abc"` vs `"cab"`

After phase 1 (count `"abc"`):

```
cnt['a'] = 1
cnt['b'] = 1
cnt['c'] = 1
all others = 0
```

After phase 2 (decrement for `"cab"`):

```
cnt['c']-- → 0
cnt['a']-- → 0
cnt['b']-- → 0
```

All zero → **anagrams.** ✓

#### Trace on `"abc"` vs `"abd"`

After phase 1: `cnt['a']=1, cnt['b']=1, cnt['c']=1`.

After phase 2: `cnt['a']--=0, cnt['b']--=0, cnt['d']--=−1`, but **cnt['c'] still 1**.

Final scan: cnt['c']=1, cnt['d']=−1 → both non-zero → **not anagrams.** ✓

**Time:** O(n + m). **Space:** O(1) (256 is constant — alphabet size). Beautiful.

## Convert string to integer (safely)

`atoi("42")` returns 42 — **but** if the input is `"abc"`, `atoi` silently returns 0, with no way to detect the error. Use `strtol` instead:

```c
int parse_int(const char *s, int *out) {
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0)        return -1;       // overflow
    if (end == s)          return -1;       // no digits at all
    if (*end != '\0')      return -1;       // trailing garbage
    if (v < INT_MIN || v > INT_MAX) return -1;
    *out = (int)v;
    return 0;
}
```

`end` is set by `strtol` to "the first character it didn't consume." Check it to validate.

| Input | `v` | `end` points to | Result |
|---|---|---|---|
| `"42"` | 42 | `'\0'` | OK |
| `"42abc"` | 42 | `'a'` | reject (trailing garbage) |
| `"abc"` | 0 | `'a'` (= s) | reject (no digits) |
| `""` | 0 | `'\0'` (= s) | reject (empty) |
| `"99999999999999"` | LONG_MAX | `'\0'` | reject (errno set) |

**Defensive parsing.** Always validate input.

## Pointer-to-literal vs array (review with picture)

```c
char *p = "hello";       // pointer to read-only memory
char a[] = "hello";      // copy of literal in writable memory
```

```
       (read-only)                      (stack)
       +---+---+---+---+---+---+        +---+---+---+---+---+---+
       | h | e | l | l | o |\0 |        | h | e | l | l | o |\0 |
       +---+---+---+---+---+---+        +---+---+---+---+---+---+
       ↑                                ↑
       p                                a
```

`p[0] = 'H'` → **crash**. `a[0] = 'H'` → fine. Different worlds.

## Common bugs (with pictures)

### Bug 1: forgetting the null terminator

You allocate a buffer of length `n` and write `n` characters into it. **Where's the `\0`?** Nowhere. `strlen` will run past the end.

```c
char buf[5];
buf[0] = 'h'; buf[1] = 'e'; buf[2] = 'l'; buf[3] = 'l'; buf[4] = 'o';
printf("%s\n", buf);                  // ← reads past the end!
```

```
        +---+---+---+---+---+
buf:    | h | e | l | l | o |  ← no '\0'!
        +---+---+---+---+---+---+---+---+...
                              | ? | ? | ? |  whatever junk is here
```

`printf` keeps printing until it accidentally hits a zero byte somewhere in junk.

**Always reserve room for `\0`.** A buffer for an n-character string needs **n+1** bytes.

### Bug 2: comparing strings with `==`

```c
char *a = "abc";
char *b = "abc";
if (a == b) ...    // compares POINTERS, not contents
```

`a == b` checks if they point to the **same** address. Sometimes the compiler shares string literals (so this might "work" by accident). **Don't rely on it.** Use `strcmp(a, b) == 0`.

### Bug 3: modifying a literal

```c
char *s = "hello";
s[0] = 'H';        // CRASH: literal is in read-only memory
```

We saw this above. Fix: `char s[] = "hello";`.

### Bug 4: `strcat` overflow

```c
char dst[10] = "hello";
strcat(dst, " world");        // "hello world" is 12 chars; only 10 in dst
```

`strcat` blindly appends past the end. Same disaster as `strcpy`. Use `snprintf` or `strscpy`.

## Recap

1. A C string = char array ending in `\0`.
2. **Length is bytes-to-`\0`-not-counting-`\0`.** Buffer must be at least length+1 bytes.
3. **Avoid** `strcpy`, `strcat`, `sprintf`, `gets`, `atoi`. **Use** `snprintf`, `strscpy`, `strtol`.
4. Pointer-to-literal is read-only. Array-from-literal is writable.
5. Common patterns: two pointers, frequency tables.

## Self-check (answer in your head)

1. How many bytes does the string `"go"` occupy?

2. Trace `is_palindrome("aa")` step by step. What does it return?

3. What's the time complexity of:
   ```c
   for (int i = 0; i < strlen(s); i++) ...
   ```
   *(Hint: `strlen(s)` is called every iteration!)*

4. After this code:
   ```c
   char a[10] = "abc";
   ```
   What's at `a[3]`? `a[5]`? `a[10]`?

5. Why does `is_anagram` use `unsigned char` when indexing `cnt`?

---

**Answers:**

1. **3 bytes**: `'g'`, `'o'`, `'\0'`.

2. n=2. i=0, j=1. s[0]='a', s[1]='a' → match. i=1, j=0 → loop ends (i<j false). Return 1 (palindrome). ✓

3. **O(n²).** Each iteration calls `strlen`, which is O(n). The loop runs n times. n × n = n². Fix: compute length once before the loop.

4. `a[3]` = `'\0'`. `a[5]` = `'\0'` (uninitialized portion is zeroed for `char a[10] = ...` — array initialization rule). `a[10]` is **out of bounds** (valid indices are 0..9). Don't read it.

5. Because `char` may be signed on some platforms. With negative values, `cnt[(int)c]` could index out of bounds (negative). Casting to `unsigned char` ensures the index is in [0, 255].

If you got 4/5, you understand strings well. If 5/5, move on.

Next → [DSA-04 — Linked lists](DSA-04-linked-lists.md).
