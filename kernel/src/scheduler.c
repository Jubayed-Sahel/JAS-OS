#include "scheduler.h"
#include "event_log.h"
#include "hw.h"
#include "klib.h"
#include "task.h"

static size_t s_next_slot;
static uint32_t s_ticks;
static bool s_paused;
static uint32_t s_pause_exempt_task;
static scheduler_policy_t s_policy;
static uint32_t s_timeline[SCHEDULER_TIMELINE_LENGTH];
static size_t s_timeline_start;
static size_t s_timeline_count;

void scheduler_init(void)
{
    s_next_slot = 0;
    s_ticks = 0;
    s_paused = false;
    s_pause_exempt_task = 0;
    s_policy = SCHEDULER_ROUND_ROBIN;
    s_timeline_start = 0;
    s_timeline_count = 0;
}

static void timeline_add(uint32_t task_id)
{
    size_t slot;
    if (s_timeline_count < SCHEDULER_TIMELINE_LENGTH) {
        slot = (s_timeline_start + s_timeline_count) % SCHEDULER_TIMELINE_LENGTH;
        ++s_timeline_count;
    } else {
        slot = s_timeline_start;
        s_timeline_start = (s_timeline_start + 1U) % SCHEDULER_TIMELINE_LENGTH;
    }
    s_timeline[slot] = task_id;
}

static bool runnable(const kernel_task_t *task)
{
    return task->state == TASK_READY && (!s_paused || task->id == s_pause_exempt_task);
}

static int choose_policy_slot(kernel_task_t *tasks)
{
    int shell_slot = -1;
    int selected = -1;
    for (size_t i = 0; i < KERNEL_MAX_TASKS; ++i) {
        if (!runnable(&tasks[i])) continue;
        if (kstrcmp(tasks[i].name, "shell") == 0) {
            shell_slot = (int)i;
            continue;
        }
        if (selected < 0) {
            selected = (int)i;
            continue;
        }
        const kernel_task_t *candidate = &tasks[i];
        const kernel_task_t *current = &tasks[selected];
        const bool better_priority = s_policy == SCHEDULER_PRIORITY &&
            (candidate->priority > current->priority ||
             (candidate->priority == current->priority && candidate->run_count < current->run_count));
        const bool better_fcfs = s_policy == SCHEDULER_FCFS && candidate->id < current->id;
        const bool better_sjf = s_policy == SCHEDULER_SJF &&
            (candidate->burst_estimate < current->burst_estimate ||
             (candidate->burst_estimate == current->burst_estimate && candidate->id < current->id));
        if (better_priority || better_fcfs || better_sjf) selected = (int)i;
    }
    if (shell_slot >= 0 && (selected < 0 || (s_ticks % 4U) == 0U)) return shell_slot;
    return selected;
}

static void run_slot(kernel_task_t *task, size_t slot)
{
    s_next_slot = (slot + 1U) % KERNEL_MAX_TASKS;
    task->state = TASK_RUNNING;
    ++task->run_count;
    ++s_ticks;
    timeline_add(task->id);
    task->step(task);
    if (task->state == TASK_RUNNING) task->state = TASK_READY;
}

void scheduler_run_one_slice(void)
{
    kernel_task_t *tasks = task_table();
    const uint32_t time = now_ms();
    for (size_t i = 0; i < KERNEL_MAX_TASKS; ++i) {
        if (tasks[i].state == TASK_SLEEPING && time >= tasks[i].wake_at_ms) tasks[i].state = TASK_READY;
    }

    if (s_policy != SCHEDULER_ROUND_ROBIN) {
        const int selected = choose_policy_slot(tasks);
        if (selected >= 0) {
            run_slot(&tasks[selected], (size_t)selected);
            return;
        }
    } else {
        for (size_t checked = 0; checked < KERNEL_MAX_TASKS; ++checked) {
            const size_t slot = (s_next_slot + checked) % KERNEL_MAX_TASKS;
            if (!runnable(&tasks[slot])) continue;
            run_slot(&tasks[slot], slot);
            return;
        }
    }
    ++s_ticks;
}

uint32_t scheduler_ticks(void) { return s_ticks; }

void scheduler_pause_except(uint32_t task_id)
{
    s_pause_exempt_task = task_id;
    s_paused = true;
    event_log_add("SCHED", "global pause enabled");
}

void scheduler_resume_all(void)
{
    s_paused = false;
    s_pause_exempt_task = 0;
    event_log_add("SCHED", "global pause released");
}

bool scheduler_is_paused(void) { return s_paused; }

bool scheduler_set_policy(scheduler_policy_t policy)
{
    if (policy > SCHEDULER_SJF) return false;
    s_policy = policy;
    event_log_add("SCHED", "policy=%s", scheduler_policy_name(policy));
    return true;
}

scheduler_policy_t scheduler_get_policy(void) { return s_policy; }

const char *scheduler_policy_name(scheduler_policy_t policy)
{
    switch (policy) {
        case SCHEDULER_ROUND_ROBIN: return "ROUND-ROBIN";
        case SCHEDULER_PRIORITY: return "PRIORITY";
        case SCHEDULER_FCFS: return "FCFS";
        case SCHEDULER_SJF: return "SJF";
        default: return "UNKNOWN";
    }
}

size_t scheduler_get_timeline(uint32_t *task_ids, size_t capacity)
{
    if (task_ids == NULL || capacity == 0) return 0;
    const size_t count = s_timeline_count < capacity ? s_timeline_count : capacity;
    const size_t skip = s_timeline_count - count;
    for (size_t i = 0; i < count; ++i) {
        task_ids[i] = s_timeline[(s_timeline_start + skip + i) % SCHEDULER_TIMELINE_LENGTH];
    }
    return count;
}
