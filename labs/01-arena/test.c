#include "arena.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char backing[1 << 16];
    arena a;
    arena_init(&a, backing, sizeof backing);

    int *xs = arena_array(&a, int, 1000);
    assert(xs);
    for (int i = 0; i < 1000; i++) xs[i] = i;

    double *ds = arena_array(&a, double, 100);
    assert(ds);
    assert(((uintptr_t)ds & (_Alignof(double) - 1)) == 0);
    for (int i = 0; i < 100; i++) ds[i] = i * 0.5;

    arena_reset(&a);
    int *ys = arena_array(&a, int, 1000);
    assert(ys == xs);   /* same address: arena rewound */

    /* exhaust */
    void *big = arena_alloc(&a, 1 << 17, 1);
    assert(big == NULL);

    printf("arena tests passed\n");
    return 0;
}
