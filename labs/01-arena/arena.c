#include "arena.h"
#include <assert.h>
#include <stdint.h>

static size_t align_up(size_t x, size_t a) {
    assert(a && (a & (a - 1)) == 0);
    return (x + a - 1) & ~(a - 1);
}

void arena_init(arena *a, void *backing, size_t cap) {
    a->base = backing;
    a->cap  = cap;
    a->off  = 0;
}

void *arena_alloc(arena *a, size_t size, size_t align) {
    size_t off = align_up(a->off, align);
    if (off + size > a->cap) return NULL;
    void *p = a->base + off;
    a->off = off + size;
    return p;
}

void arena_reset(arena *a) { a->off = 0; }
