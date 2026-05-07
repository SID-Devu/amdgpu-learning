#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

typedef struct arena {
    char  *base;
    size_t cap;
    size_t off;
} arena;

void  arena_init(arena *a, void *backing, size_t cap);
void *arena_alloc(arena *a, size_t size, size_t align);
void  arena_reset(arena *a);

#define arena_new(a, T) ((T*)arena_alloc((a), sizeof(T), _Alignof(T)))
#define arena_array(a, T, n) ((T*)arena_alloc((a), sizeof(T)*(n), _Alignof(T)))

#endif
