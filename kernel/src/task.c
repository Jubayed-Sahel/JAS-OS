#include "task.h"
#include "event_log.h"
#include "hw.h"
#include "klib.h"

static kernel_task_t s_tasks[KERNEL_MAX_TASKS];
static uint32_t s_next_id = 1;

void task_system_init(void)
{
    kmemset(s_tasks, 0, sizeof(s_tasks));
    s_next_id = 1;
}

int task_create(const char *name, task_step_fn step, void *context)
{
    if (name == NULL || step == NULL) return -1;

    for (size_t i = 0; i < KERNEL_MAX_TASKS; ++i) {
        if (s_tasks[i].state != TASK_UNUSED && s_tasks[i].state != TASK_FINISHED) continue;

        kernel_task_t *task = &s_tasks[i];
        kmemset(task, 0, sizeof(*task));
        task->id = s_next_id++;
        ksnprintf(task->name, sizeof(task->name), "%s", name);
        task->state = TASK_READY;
        task->step = step;
        task->context = context;
        task->priority = kstrcmp(name, "shell") == 0 ? 9U : 5U;
        task->burst_estimate = kstrcmp(name, "counter") == 0 ? 4U : 3U;
        event_log_add("TASK", "created %s (id=%u)", task->name, task->id);
        return (int)task->id;
    }
    return -1;
}

kernel_task_t *task_get(uint32_t id)
{
    for (size_t i = 0; i < KERNEL_MAX_TASKS; ++i) {
        if (s_tasks[i].state != TASK_UNUSED && s_tasks[i].id == id) return &s_tasks[i];
    }
    return NULL;
}

bool task_kill(uint32_t id)
{
    kernel_task_t *task = task_get(id);
    if (task == NULL) return false;
    task->state = TASK_FINISHED;
    event_log_add("TASK", "killed %s (id=%u)", task->name, id);
    return true;
}

bool task_suspend(uint32_t id)
{
    kernel_task_t *task = task_get(id);
    if (task == NULL || task->state == TASK_FINISHED) return false;
    task->state = TASK_SUSPENDED;
    event_log_add("TASK", "suspended %s (id=%u)", task->name, id);
    return true;
}

bool task_resume(uint32_t id)
{
    kernel_task_t *task = task_get(id);
    if (task == NULL || task->state != TASK_SUSPENDED) return false;
    task->state = TASK_READY;
    event_log_add("TASK", "resumed %s (id=%u)", task->name, id);
    return true;
}

void task_sleep(kernel_task_t *task, uint32_t milliseconds)
{
    if (task == NULL) return;
    task->wake_at_ms = now_ms() + milliseconds;
    task->state = TASK_SLEEPING;
}

kernel_task_t *task_table(void) { return s_tasks; }

const char *task_state_name(task_state_t state)
{
    switch (state) {
        case TASK_UNUSED: return "UNUSED";
        case TASK_READY: return "READY";
        case TASK_RUNNING: return "RUNNING";
        case TASK_SLEEPING: return "SLEEPING";
        case TASK_BLOCKED: return "BLOCKED";
        case TASK_SUSPENDED: return "SUSPENDED";
        case TASK_FINISHED: return "FINISHED";
        default: return "UNKNOWN";
    }
}

bool task_set_priority(uint32_t id, uint8_t priority)
{
    kernel_task_t *task = task_get(id);
    if (task == NULL || priority > 9U) return false;
    task->priority = priority;
    event_log_add("SCHED", "task %u priority=%u", id, priority);
    return true;
}

bool task_set_burst(uint32_t id, uint16_t burst_estimate)
{
    kernel_task_t *task = task_get(id);
    if (task == NULL || burst_estimate == 0U) return false;
    task->burst_estimate = burst_estimate;
    event_log_add("SCHED", "task %u burst=%u", id, burst_estimate);
    return true;
}

size_t task_active_count(void)
{
    size_t count = 0;
    for (size_t i = 0; i < KERNEL_MAX_TASKS; ++i) {
        if (s_tasks[i].state != TASK_UNUSED && s_tasks[i].state != TASK_FINISHED) count++;
    }
    return count;
}
