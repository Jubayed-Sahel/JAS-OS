#ifndef MINI_KERNEL_SYNC_H
#define MINI_KERNEL_SYNC_H

#include "task.h"

typedef struct {
    int value;
    uint32_t waiting_ids[KERNEL_MAX_TASKS];
    size_t waiting_count;
} kernel_semaphore_t;

void semaphore_init(kernel_semaphore_t *semaphore, int initial_value);
bool semaphore_wait(kernel_semaphore_t *semaphore, kernel_task_t *task);
void semaphore_signal(kernel_semaphore_t *semaphore);

#endif
