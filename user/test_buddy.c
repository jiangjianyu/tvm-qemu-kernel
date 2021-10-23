
#include <stdio.h>
#include <assert.h>
#define BUDDY_ALLOC_IMPLEMENTATION
#include "buddy.h"
#include <malloc.h>

int main() {
    size_t arena_size = 4096 * 100000;
    /* You need space for the metadata and for the arena */
    void *buddy_metadata = malloc(buddy_sizeof(arena_size));
    void *buddy_arena = malloc(arena_size);
    struct buddy *buddy = buddy_init(buddy_metadata, buddy_arena, arena_size);

    /* Allocate using the buddy allocator */
    void *data = buddy_malloc(buddy, 4096 * 49);
    void *data1 = buddy_malloc(buddy, 4096 * 9);
    void *data2 = buddy_malloc(buddy, 4096 * 1);
    void *data3 = buddy_malloc(buddy, 4096 * 8192);
    void *data4 = buddy_malloc(buddy, 4096 * 8192);

    printf("result %lx %lx %lx %lx %lx\n", data, data1, data2, data3, data4);
    /* Free using the buddy allocator */
    buddy_free(buddy, data);

    free(buddy_metadata);
    free(buddy_arena);
}