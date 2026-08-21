#ifndef MINI_KERNEL_TASK_H
#define MINI_KERNEL_TASK_H

#include "ktypes.h"

#define KERNEL_MAX_TASKS 8
#define KERNEL_TASK_NAME_LENGTH 16

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_BLOCKED,
    TASK_SUSPENDED,
    TASK_FINISHED
} task_state_t;

struct kernel_task;

typedef void (*task_step_fn)(struct kernel_task *task);

typedef struct kernel_task {
    uint32_t id;
    char name[KERNEL_TASK_NAME_LENGTH];
    task_state_t state;
    task_step_fn step;
    void *context;
    uint32_t wake_at_ms;
    uint32_t run_count;
    uint8_t priority;
    uint16_t burst_estimate;
} kernel_task_t;

void task_system_init(void);
int task_create(const char *name, task_step_fn step, void *context);
bool task_kill(uint32_t id);
bool task_suspend(uint32_t id);
bool task_resume(uint32_t id);
void task_sleep(kernel_task_t *task, uint32_t milliseconds);
kernel_task_t *task_get(uint32_t id);
kernel_task_t *task_table(void);
const char *task_state_name(task_state_t state);
bool task_set_priority(uint32_t id, uint8_t priority);
bool task_set_burst(uint32_t id, uint16_t burst_estimate);
size_t task_active_count(void);

#endif
