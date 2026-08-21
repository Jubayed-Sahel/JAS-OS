#include "memory.h"
#include "klib.h"

#define KERNEL_HEAP_SIZE (32U * 1024U)
#define HEAP_ALIGNMENT 8U
#define BLOCK_MAGIC 0xB10CDA7AU

typedef struct memory_block {
    size_t size;
    struct memory_block *next;
    uint32_t magic;
    bool is_free;
} memory_block_t;

static uint8_t s_heap[KERNEL_HEAP_SIZE] __attribute__((aligned(8)));
static memory_block_t *s_first_block;

static size_t align_size(size_t size)
{
    return (size + HEAP_ALIGNMENT - 1U) & ~(HEAP_ALIGNMENT - 1U);
}

static bool pointer_inside_heap(const void *pointer)
{
    const uint8_t *byte = (const uint8_t *)pointer;
    return byte >= s_heap && byte < s_heap + sizeof(s_heap);
}

void memory_init(void)
{
    kmemset(s_heap, 0, sizeof(s_heap));
    s_first_block = (memory_block_t *)s_heap;
    s_first_block->size = sizeof(s_heap) - sizeof(memory_block_t);
    s_first_block->next = NULL;
    s_first_block->magic = BLOCK_MAGIC;
    s_first_block->is_free = true;
}

void *kernel_malloc(size_t requested_size)
{
    if (requested_size == 0 || s_first_block == NULL) return NULL;
    const size_t size = align_size(requested_size);
    for (memory_block_t *block = s_first_block; block != NULL; block = block->next) {
        if (!block->is_free || block->size < size) continue;
        if (block->size >= size + sizeof(memory_block_t) + HEAP_ALIGNMENT) {
            uint8_t *remainder_address = (uint8_t *)(block + 1) + size;
            memory_block_t *remainder = (memory_block_t *)remainder_address;
            remainder->size = block->size - size - sizeof(memory_block_t);
            remainder->next = block->next;
            remainder->magic = BLOCK_MAGIC;
            remainder->is_free = true;
            block->size = size;
            block->next = remainder;
        }
        block->is_free = false;
        return (void *)(block + 1);
    }
    return NULL;
}

void *kernel_calloc(size_t count, size_t size)
{
    if (count != 0 && size > (size_t)-1 / count) return NULL;
    const size_t total = count * size;
    void *pointer = kernel_malloc(total);
    if (pointer != NULL) kmemset(pointer, 0, total);
    return pointer;
}

bool kernel_free(void *pointer)
{
    if (pointer == NULL || !pointer_inside_heap(pointer)) return false;
    memory_block_t *block = ((memory_block_t *)pointer) - 1;
    if (!pointer_inside_heap(block) || block->magic != BLOCK_MAGIC || block->is_free) return false;
    block->is_free = true;
    memory_block_t *current = s_first_block;
    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
            current->size += sizeof(memory_block_t) + current->next->size;
            current->next = current->next->next;
            continue;
        }
        current = current->next;
    }
    return true;
}

kernel_heap_stats_t memory_get_stats(void)
{
    kernel_heap_stats_t stats;
    kmemset(&stats, 0, sizeof(stats));
    stats.heap_size = sizeof(s_heap);
    for (memory_block_t *block = s_first_block; block != NULL; block = block->next) {
        if (block->is_free) {
            stats.free_bytes += block->size;
            stats.free_blocks++;
            if (block->size > stats.largest_free_block) stats.largest_free_block = block->size;
        } else {
            stats.allocated_bytes += block->size;
            stats.allocated_blocks++;
        }
    }
    return stats;
}

bool memory_self_test(void)
{
    void *first = kernel_malloc(64);
    void *second = kernel_calloc(16, 8);
    void *third = kernel_malloc(256);
    if (first == NULL || second == NULL || third == NULL) return false;
    return kernel_free(second) && kernel_free(first) && kernel_free(third);
}
