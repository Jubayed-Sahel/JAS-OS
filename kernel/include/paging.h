#ifndef MINI_KERNEL_PAGING_H
#define MINI_KERNEL_PAGING_H

#include "ktypes.h"

#define PAGING_PROCESS_COUNT 3U
#define PAGING_VIRTUAL_PAGES 8U
#define PAGING_FRAME_COUNT 6U
#define PAGING_PAGE_SIZE 256U

typedef struct {
    bool present;
    bool dirty;
    uint8_t frame;
} paging_page_entry_t;

typedef struct {
    bool occupied;
    bool dirty;
    uint8_t process;
    uint8_t page;
    uint32_t loaded_at;
    uint32_t last_used;
} paging_frame_t;

typedef struct {
    uint32_t accesses;
    uint32_t hits;
    uint32_t faults;
    uint32_t evictions;
    uint32_t writebacks;
} paging_stats_t;

typedef struct {
    uint8_t process;
    uint8_t page;
    uint16_t offset;
    uint8_t frame;
    uint16_t physical_address;
    bool write;
    bool page_fault;
    bool evicted;
    bool victim_dirty;
    uint8_t victim_process;
    uint8_t victim_page;
} paging_result_t;

void paging_init(void);
bool paging_translate(uint8_t process, uint16_t virtual_address, bool write,
                      paging_result_t *result);
paging_stats_t paging_get_stats(void);
paging_page_entry_t paging_get_page(uint8_t process, uint8_t page);
paging_frame_t paging_get_frame(uint8_t frame);
bool paging_self_test(void);

#endif
