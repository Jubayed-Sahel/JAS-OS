#include "sync.h"
#include "klib.h"

static bool queue_contains(const kernel_semaphore_t *semaphore, uint32_t task_id)
{
    for (size_t i = 0; i < semaphore->waiting_count; ++i) {
        if (semaphore->waiting_ids[i] == task_id) return true;
    }
    return false;
}

static void remove_first_waiter(kernel_semaphore_t *semaphore)
{
    if (semaphore->waiting_count == 0) return;
    for (size_t i = 1; i < semaphore->waiting_count; ++i) {
        semaphore->waiting_ids[i - 1] = semaphore->waiting_ids[i];
    }
    semaphore->waiting_count--;
}

void semaphore_init(kernel_semaphore_t *semaphore, int initial_value)
{
    if (semaphore == NULL) return;
    kmemset(semaphore, 0, sizeof(*semaphore));
    semaphore->value = initial_value < 0 ? 0 : initial_value;
}

bool semaphore_wait(kernel_semaphore_t *semaphore, kernel_task_t *task)
{
    if (semaphore == NULL || task == NULL) return false;
    const bool first_waiter = semaphore->waiting_count > 0 &&
                              semaphore->waiting_ids[0] == task->id;
    if (semaphore->value > 0 && (semaphore->waiting_count == 0 || first_waiter)) {
        semaphore->value--;
        if (first_waiter) remove_first_waiter(semaphore);
        return true;
    }
    if (!queue_contains(semaphore, task->id) && semaphore->waiting_count < KERNEL_MAX_TASKS) {
        semaphore->waiting_ids[semaphore->waiting_count++] = task->id;
    }
    task->state = TASK_BLOCKED;
    return false;
}

void semaphore_signal(kernel_semaphore_t *semaphore)
{
    if (semaphore == NULL) return;
    semaphore->value++;
    while (semaphore->waiting_count > 0) {
        kernel_task_t *task = task_get(semaphore->waiting_ids[0]);
        if (task == NULL || task->state == TASK_FINISHED || task->state == TASK_UNUSED) {
            remove_first_waiter(semaphore);
            continue;
        }
        if (task->state == TASK_BLOCKED) task->state = TASK_READY;
        break;
    }
}
