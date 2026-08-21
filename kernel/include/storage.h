#ifndef MINI_KERNEL_STORAGE_H
#define MINI_KERNEL_STORAGE_H

#include "ktypes.h"

#define DISK_MAX_REQUESTS 16
#define DISK_MAX_CYLINDER 199

typedef enum {
    DISK_FCFS = 0,
    DISK_SSTF,
    DISK_SCAN,
    DISK_CSCAN,
    DISK_CLOOK
} disk_policy_t;

typedef struct {
    uint16_t order[DISK_MAX_REQUESTS + 2];
    size_t count;
    uint32_t head_movement;
} disk_result_t;

typedef enum {
    IO_POLLING = 0,
    IO_INTERRUPT,
    IO_DMA
} io_mode_t;

typedef struct {
    uint32_t requests;
    uint32_t bytes;
    uint32_t polls;
    uint32_t interrupts;
    uint32_t dma_transfers;
} io_stats_t;

void storage_init(void);
const char *disk_policy_name(disk_policy_t policy);
bool disk_schedule(disk_policy_t policy, const uint16_t *requests, size_t count,
                   uint16_t head, bool direction_up, disk_result_t *result);
void io_simulate(io_mode_t mode, uint32_t bytes);
io_stats_t io_get_stats(void);
void io_reset_stats(void);

#endif
