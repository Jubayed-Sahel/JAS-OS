#ifndef MINI_KERNEL_SCHEDULER_H
#define MINI_KERNEL_SCHEDULER_H

#include "ktypes.h"

typedef enum {
    SCHEDULER_ROUND_ROBIN = 0,
    SCHEDULER_PRIORITY,
    SCHEDULER_FCFS,
    SCHEDULER_SJF,
} scheduler_policy_t;

#define SCHEDULER_TIMELINE_LENGTH 24U

void scheduler_init(void);
void scheduler_run_one_slice(void);
uint32_t scheduler_ticks(void);
void scheduler_pause_except(uint32_t task_id);
void scheduler_resume_all(void);
bool scheduler_is_paused(void);
bool scheduler_set_policy(scheduler_policy_t policy);
scheduler_policy_t scheduler_get_policy(void);
const char *scheduler_policy_name(scheduler_policy_t policy);
size_t scheduler_get_timeline(uint32_t *task_ids, size_t capacity);

#endif
