#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct ArenaRegion {
    size_t count;
    size_t capacity;
    struct ArenaRegion *next;
    uintptr_t items[];
} ArenaRegion;

#define ALL_REGION_MIN_SIZE (4096 - sizeof(ArenaRegion))
typedef struct {
    ArenaRegion *start, *end;
} ArenaAllocator;

void *armalloc(ArenaAllocator *a, size_t size);
void arfree(ArenaAllocator *a);

void *armalloc(ArenaAllocator *a, size_t size) {
    if (size == 0) return NULL;
    size = (size + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
    if (!a->end || a->end->count + size > a->end->capacity) {
        size_t region_size = sizeof(ArenaRegion) + (size > ALL_REGION_MIN_SIZE ? size : ALL_REGION_MIN_SIZE);
        ArenaRegion *r = malloc(region_size);
        if (!r) return NULL;
        r->count = 0;
        r->capacity = region_size - sizeof(ArenaRegion);
        r->next = NULL;
        if (a->end) {
            a->end->next = r;
            a->end = r;
        } else {
            a->start = a->end = r;
        }
    }
    void *ptr = (void *)((uintptr_t)a->end->items + a->end->count);
    a->end->count += size;
    memset(ptr, 0, size);
    return ptr;
}

void arfree(ArenaAllocator *a) {
    ArenaRegion *r = a->start;
    while (r) {
        ArenaRegion *next = r->next;
        free(r);
        r = next;
    }
    a->start = a->end = NULL;
}