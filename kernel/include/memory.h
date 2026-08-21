#ifndef MINI_KERNEL_MEMORY_H
#define MINI_KERNEL_MEMORY_H

#include "ktypes.h"

typedef struct {
    size_t heap_size;
    size_t allocated_bytes;
    size_t free_bytes;
    size_t largest_free_block;
    size_t allocated_blocks;
    size_t free_blocks;
} kernel_heap_stats_t;

void memory_init(void);
void *kernel_malloc(size_t size);
void *kernel_calloc(size_t count, size_t size);
bool kernel_free(void *pointer);
kernel_heap_stats_t memory_get_stats(void);
bool memory_self_test(void);

#endif
