# DSA 3 — Strings (in C, properly)

> "C strings are not strings. They are pointers + a convention. Master both."

A C string is just a `char *` to a buffer that ends with a `'\0'` (null terminator). That's it. Every gnarly bug you'll ever have with strings in C comes from forgetting one of:

- **The `\0`** is a real byte.
- **The buffer must be big enough** for length + 1.
- **`strlen` is `O(n)`** — it scans till `\0`.
- **There is no automatic resizing.** You write past the end → silent corruption or crash.

## Anatomy of a C string

```c
char s[] = "hi";
```

In memory: `'h' 'i' '\0'` — three bytes, not two. `sizeof(s) == 3`.

```c
char s[6] = "hi";   // 'h' 'i' '\0' '\0' '\0' '\0'
```

The unused tail is zeroed (in static/automatic init). Don't rely on it for security-sensitive contexts; clear it explicitly.

## The standard library — what to know, what to avoid

| Function | Use? | Why |
|---|---|---|
| `strlen(s)` | yes | Length, excluding `\0`. O(n). |
| `strcpy(dst, src)` | **avoid** | No bounds check; classic buffer overflow. |
| `strncpy(dst, src, n)` | **avoid** | Doesn't always null-terminate; trickier than it looks. |
| `strlcpy(dst, src, n)` (BSD/Linux) | yes | Always null-terminates, returns intended length. |
| `snprintf(dst, n, "...")` | yes | Best general-purpose; always null-terminates. |
| `strcmp(a, b)` | yes | Lexicographic compare. Returns 0 if equal. |
| `strncmp(a, b, n)` | yes | Compare first n chars. |
| `strchr(s, c)` | yes | First occurrence of c. |
| `strstr(haystack, needle)` | yes | First substring; O(n*m) naive. |
| `strtok(s, delim)` | careful | Modifies its input; not thread-safe. Use `strtok_r`. |
| `atoi`, `atof` | **avoid** | No error indication. Use `strtol`, `strtod`. |

**Rule**: prefer `snprintf` for every "build a string" task. Avoid `strcpy`/`sprintf` entirely.

## Hand-rolling the basics

### strlen
```c
size_t my_strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}
```

### strcmp
```c
int my_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
```

(Why `unsigned char`? So that high-bit chars compare consistently across signed/unsigned `char` defaults.)

### strcpy with bounds (use this pattern, not strcpy)
```c
size_t safe_strcpy(char *dst, size_t cap, const char *src) {
    if (cap == 0) return 0;
    size_t i = 0;
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return i;
}
```

### strstr (naive)
```c
const char *my_strstr(const char *hay, const char *needle) {
    if (!*needle) return hay;
    for (size_t i = 0; hay[i]; i++) {
        size_t j = 0;
        while (needle[j] && hay[i+j] == needle[j]) j++;
        if (!needle[j]) return hay + i;
    }
    return NULL;
}
```
O(n*m). KMP brings it to O(n+m); we'll do that in DSA-25.

## Common string algorithms / patterns

### Reverse a string in place
```c
void reverse_str(char *s) {
    size_t n = strlen(s);
    for (size_t i = 0, j = n - 1; i < j; i++, j--) {
        char t = s[i]; s[i] = s[j]; s[j] = t;
    }
}
```

### Check palindrome
```c
int is_palindrome(const char *s) {
    size_t n = strlen(s);
    for (size_t i = 0, j = n - 1; i < j; i++, j--)
        if (s[i] != s[j]) return 0;
    return 1;
}
```

### Anagram check (counts of each char)
```c
int is_anagram(const char *a, const char *b) {
    int cnt[256] = {0};
    while (*a) cnt[(unsigned char)*a++]++;
    while (*b) cnt[(unsigned char)*b++]--;
    for (int i = 0; i < 256; i++) if (cnt[i]) return 0;
    return 1;
}
```
O(n + m), O(1) extra space (256 is constant). For Unicode you'd need a bigger structure (hash map of code points).

### Count occurrences of each char (frequency)
Same `cnt[256]` pattern. Used everywhere — frequency tables, histograms.

### Tokenize ("split")
```c
// Splits s on delim. Modifies s.
char *tok = strtok_r(s, " ", &saveptr);
while (tok) {
    // use tok
    tok = strtok_r(NULL, " ", &saveptr);
}
```

### Convert string to integer with error checking
```c
#include <errno.h>
#include <stdlib.h>

int parse_int(const char *s, int *out) {
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || *end || end == s) return -1;
    if (v < INT_MIN || v > INT_MAX) return -1;
    *out = (int)v;
    return 0;
}
```

## Strings as immutable vs mutable

```c
char *p = "hello";    // pointer to read-only memory (literal)
p[0] = 'H';           // SEGFAULT — undefined behavior

char a[] = "hello";   // copy of literal into modifiable array
a[0] = 'H';           // OK — a is "Hello"
```

**Rule**: if you intend to modify, declare an `array`, not a pointer-to-literal.

## Encoding gotchas (cross-link to A5)

- ASCII fits in 1 byte per char. Lengths in bytes == lengths in chars.
- **UTF-8** uses 1–4 bytes per Unicode code point. `strlen` returns byte length, **not character count**. For "true" character count, you must walk and count UTF-8 sequences.
- A naive "reverse" of a UTF-8 string can produce invalid UTF-8.

For 90% of systems work, you don't need Unicode awareness. But know it exists. (See [Foundations A5](../part0-zero/foundations/A5-character-encoding.md).)

## Strings in the kernel

- `kstrdup(s, GFP_KERNEL)` — allocate + copy. Free with `kfree`.
- `scnprintf(buf, size, fmt, ...)` — like `snprintf` but returns the actual bytes written, not the would-be length. Useful when building sysfs/debugfs output.
- `strscpy(dst, src, sz)` — preferred over `strncpy` and `strlcpy` in new kernel code. Always null-terminates; clearly indicates truncation.
- `kasprintf(GFP_KERNEL, fmt, ...)` — printf-style allocate-and-format. Free with `kfree`.

In the kernel you almost never use `malloc`/`free`/`stdio.h`; we'll learn the kernel API in Part V.

## Common string bugs

1. **Off-by-one with terminator** — buffer of `n` characters needs `n+1` bytes.
2. **`strlen` on non-null-terminated buffer** — runs forever or segfaults.
3. **Comparing with `==`** — `s1 == s2` compares pointers, not contents. Use `strcmp`.
4. **Concat overflow** — `strcat` can run off the end of `dst`. Use `snprintf` instead.
5. **Modifying a literal** — undefined behavior.
6. **`sprintf` for untrusted format strings** — security disaster (format string vulnerability).

## Worked example: simple log parser

Read a line like `[12345] gpu_reset start` and extract the number and the message.

```c
int parse_log(const char *line, int *ts, char *msg, size_t mcap) {
    if (line[0] != '[') return -1;
    char *end;
    long v = strtol(line + 1, &end, 10);
    if (*end != ']' || end[1] != ' ') return -1;
    *ts = (int)v;
    return safe_strcpy(msg, mcap, end + 2) ? 0 : -1;
}
```

Notice: no `strcpy`, no `atoi`. Defensive parsing. Robust under fuzzed input.

## Try it

1. Implement `safe_strcpy`, `my_strlen`, `my_strcmp`, `my_strstr` from scratch. Test against libc on edge cases (empty string, single char, long string).
2. Implement `is_anagram` for ASCII; then for "case-insensitive English." Then think (don't code) how it'd change for full Unicode.
3. Implement `int char_freq(const char *s, int *out256)` to fill a 256-entry frequency table.
4. Read the man page of `snprintf`. Find the trick to compute "how big a buffer do I need?" *(Hint: pass `NULL`, `0`, then read return value.)*
5. Read `lib/string.c` in the Linux kernel. Compare its `strscpy` to your `safe_strcpy`. What does it do that yours doesn't?

## Read deeper

- *The C Programming Language* (K&R), chapter on strings.
- `man 3 string` and `man 3 snprintf`.
- "A look at how strings work in the Linux kernel" — search `lwn.net` for `strscpy`.

Next → [Linked lists](DSA-04-linked-lists.md).
