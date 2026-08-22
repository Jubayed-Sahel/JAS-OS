#include "demos.h"
#include "event_log.h"
#include "klib.h"
#include "sync.h"
#include "task.h"

typedef struct { uint32_t value; } counter_context_t;
typedef struct { uint32_t phase; } pipeline_context_t;
typedef struct { uint32_t worker; } thread_context_t;
typedef struct { uint32_t phase; uint32_t number; } rw_context_t;

static counter_context_t s_counter_contexts[KERNEL_MAX_TASKS];
static size_t s_counter_count;
static int s_buffer[DEMO_BUFFER_SIZE];
static size_t s_buffer_in, s_buffer_out, s_buffer_count;
static int s_next_item = 1;
static bool s_pipeline_started, s_pipeline_paused;
static uint32_t s_producer_id, s_consumer_id;
static pipeline_context_t s_producer_context, s_consumer_context;
static kernel_semaphore_t s_empty_slots, s_full_slots, s_buffer_mutex;
static char s_log[DEMO_LOG_CAP][80];
static size_t s_log_start, s_log_count;

/* Two schedulable workers sharing one address-space value. */
static bool s_threads_started;
static uint32_t s_thread_ids[2], s_shared_counter;
static thread_context_t s_thread_contexts[2];
static kernel_semaphore_t s_thread_mutex;

/* Classic first-readers-writers semaphore solution. */
static bool s_rw_started;
static uint32_t s_reader_ids[2], s_writer_id, s_reader_count, s_rw_value;
static rw_context_t s_reader_contexts[2], s_writer_context;
static kernel_semaphore_t s_reader_mutex, s_rw_resource;

static void demo_log(const char *fmt, ...)
{
    size_t slot;
    if (s_log_count < DEMO_LOG_CAP) {
        slot = (s_log_start + s_log_count) % DEMO_LOG_CAP;
        s_log_count++;
    } else {
        slot = s_log_start;
        s_log_start = (s_log_start + 1) % DEMO_LOG_CAP;
    }
    va_list args;
    va_start(args, fmt);
    kvsnprintf(s_log[slot], sizeof(s_log[slot]), fmt, args);
    va_end(args);
    kprintf("%s\n", s_log[slot]);
}

static void counter_step(kernel_task_t *task)
{
    counter_context_t *context = (counter_context_t *)task->context;
    kprintf("[counter] task %u | %s | value = %u\n", task->id, task->name, context->value++);
    task_sleep(task, 1000);
}

int demo_create_counter_named(const char *name)
{
    if (s_counter_count >= KERNEL_MAX_TASKS) return -1;
    counter_context_t *context = &s_counter_contexts[s_counter_count++];
    context->value = 0;
    const int id = task_create(name && name[0] ? name : "counter", counter_step, context);
    if (id < 0) s_counter_count--;
    return id;
}

int demo_create_counter(void)
{
    return demo_create_counter_named("counter");
}

static void producer_step(kernel_task_t *task)
{
    if (s_pipeline_paused) { task_sleep(task, 100); return; }
    pipeline_context_t *context = (pipeline_context_t *)task->context;
    if (context->phase == 0) {
        if (!semaphore_wait(&s_empty_slots, task)) return;
        context->phase = 1;
    }
    if (!semaphore_wait(&s_buffer_mutex, task)) return;
    const int item = s_next_item++;
    s_buffer[s_buffer_in] = item;
    s_buffer_in = (s_buffer_in + 1U) % DEMO_BUFFER_SIZE;
    s_buffer_count++;
    demo_log("[produce +] item %d | buffer [%u/%u]", item, (unsigned)s_buffer_count, DEMO_BUFFER_SIZE);
    semaphore_signal(&s_buffer_mutex);
    semaphore_signal(&s_full_slots);
    context->phase = 0;
    task_sleep(task, 700);
}

static void consumer_step(kernel_task_t *task)
{
    if (s_pipeline_paused) { task_sleep(task, 100); return; }
    pipeline_context_t *context = (pipeline_context_t *)task->context;
    if (context->phase == 0) {
        if (!semaphore_wait(&s_full_slots, task)) return;
        context->phase = 1;
    }
    if (!semaphore_wait(&s_buffer_mutex, task)) return;
    const int item = s_buffer[s_buffer_out];
    s_buffer_out = (s_buffer_out + 1U) % DEMO_BUFFER_SIZE;
    s_buffer_count--;
    demo_log("[consume -] item %d | buffer [%u/%u]", item, (unsigned)s_buffer_count, DEMO_BUFFER_SIZE);
    semaphore_signal(&s_buffer_mutex);
    semaphore_signal(&s_empty_slots);
    context->phase = 0;
    task_sleep(task, 1100);
}

bool demo_start_producer_consumer(void)
{
    if (s_pipeline_started) return false;
    kmemset(s_buffer, 0, sizeof(s_buffer));
    s_buffer_in = s_buffer_out = s_buffer_count = 0;
    s_next_item = 1;
    s_pipeline_paused = false;
    s_producer_context.phase = 0;
    s_consumer_context.phase = 0;
    semaphore_init(&s_empty_slots, DEMO_BUFFER_SIZE);
    semaphore_init(&s_full_slots, 0);
    semaphore_init(&s_buffer_mutex, 1);
    const int producer_id = task_create("producer", producer_step, &s_producer_context);
    const int consumer_id = task_create("consumer", consumer_step, &s_consumer_context);
    if (producer_id < 0 || consumer_id < 0) {
        if (producer_id >= 0) task_kill((uint32_t)producer_id);
        if (consumer_id >= 0) task_kill((uint32_t)consumer_id);
        return false;
    }
    s_pipeline_started = true;
    s_producer_id = (uint32_t)producer_id;
    s_consumer_id = (uint32_t)consumer_id;
    event_log_add("DEMO", "producer-consumer started (%u,%u)", s_producer_id, s_consumer_id);
    return true;
}

bool demo_pause_producer_consumer(void)
{
    if (!s_pipeline_started || s_pipeline_paused) return false;
    s_pipeline_paused = true;
    event_log_add("DEMO", "producer-consumer paused");
    return true;
}

bool demo_resume_producer_consumer(void)
{
    if (!s_pipeline_started || !s_pipeline_paused) return false;
    s_pipeline_paused = false;
    event_log_add("DEMO", "producer-consumer resumed");
    return true;
}

bool demo_stop_producer_consumer(void)
{
    if (!s_pipeline_started) return false;
    task_kill(s_producer_id);
    task_kill(s_consumer_id);
    s_pipeline_started = false;
    s_pipeline_paused = false;
    s_producer_id = s_consumer_id = 0;
    event_log_add("DEMO", "producer-consumer stopped");
    return true;
}

void demo_get_pc_status(demo_pc_status_t *out)
{
    if (!out) return;
    kmemset(out, 0, sizeof(*out));
    out->started = s_pipeline_started;
    out->paused = s_pipeline_paused;
    out->producer_id = s_producer_id;
    out->consumer_id = s_consumer_id;
    out->buffer_count = s_buffer_count;
    kmemcpy(out->buffer, s_buffer, sizeof(s_buffer));
    out->empty_slots = s_empty_slots.value;
    out->full_slots = s_full_slots.value;
    out->mutex = s_buffer_mutex.value;
}

size_t demo_log_count(void) { return s_log_count; }

const char *demo_log_get(size_t newest_offset)
{
    if (newest_offset >= s_log_count) return "";
    const size_t logical = s_log_count - 1U - newest_offset;
    return s_log[(s_log_start + logical) % DEMO_LOG_CAP];
}

static void thread_worker_step(kernel_task_t *task)
{
    thread_context_t *context = (thread_context_t *)task->context;
    if (!semaphore_wait(&s_thread_mutex, task)) return;
    ++s_shared_counter;
    if ((s_shared_counter % 5U) == 0U)
        demo_log("[thread %c] shared counter = %u", context->worker ? 'B' : 'A', s_shared_counter);
    semaphore_signal(&s_thread_mutex);
    task_sleep(task, context->worker ? 420U : 300U);
}

bool demo_start_threads(void)
{
    if (s_threads_started) return false;
    s_shared_counter = 0;
    s_thread_contexts[0].worker = 0;
    s_thread_contexts[1].worker = 1;
    semaphore_init(&s_thread_mutex, 1);
    int a = task_create("thread-A", thread_worker_step, &s_thread_contexts[0]);
    int b = task_create("thread-B", thread_worker_step, &s_thread_contexts[1]);
    if (a < 0 || b < 0) {
        if (a >= 0) task_kill((uint32_t)a);
        if (b >= 0) task_kill((uint32_t)b);
        return false;
    }
    s_thread_ids[0] = (uint32_t)a;
    s_thread_ids[1] = (uint32_t)b;
    s_threads_started = true;
    event_log_add("THREAD", "shared workers started (%u,%u)", s_thread_ids[0], s_thread_ids[1]);
    return true;
}

bool demo_stop_threads(void)
{
    if (!s_threads_started) return false;
    task_kill(s_thread_ids[0]);
    task_kill(s_thread_ids[1]);
    s_threads_started = false;
    event_log_add("THREAD", "shared workers stopped");
    return true;
}

void demo_get_thread_status(demo_thread_status_t *out)
{
    if (!out) return;
    out->started = s_threads_started;
    out->worker_a_id = s_thread_ids[0];
    out->worker_b_id = s_thread_ids[1];
    out->shared_counter = s_shared_counter;
}

static void reader_step(kernel_task_t *task)
{
    rw_context_t *context = (rw_context_t *)task->context;
    if (context->phase == 0) {
        if (!semaphore_wait(&s_reader_mutex, task)) return;
        context->phase = 1;
    }
    if (context->phase == 1) {
        if (s_reader_count == 0 && !semaphore_wait(&s_rw_resource, task)) return;
        ++s_reader_count;
        semaphore_signal(&s_reader_mutex);
        demo_log("[reader %u] enter | readers=%u | value=%u",
                 context->number, s_reader_count, s_rw_value);
        context->phase = 2;
        task_sleep(task, 500);
        return;
    }
    if (context->phase == 2) {
        if (!semaphore_wait(&s_reader_mutex, task)) return;
        context->phase = 3;
    }
    if (context->phase == 3) {
        if (s_reader_count) --s_reader_count;
        if (s_reader_count == 0) semaphore_signal(&s_rw_resource);
        semaphore_signal(&s_reader_mutex);
        demo_log("[reader %u] leave | readers=%u", context->number, s_reader_count);
        context->phase = 0;
        task_sleep(task, 900U + context->number * 140U);
    }
}

static void writer_step(kernel_task_t *task)
{
    rw_context_t *context = (rw_context_t *)task->context;
    if (context->phase == 0) {
        if (!semaphore_wait(&s_rw_resource, task)) return;
        ++s_rw_value;
        demo_log("[writer] exclusive write | value=%u", s_rw_value);
        context->phase = 1;
        task_sleep(task, 650);
        return;
    }
    semaphore_signal(&s_rw_resource);
    context->phase = 0;
    task_sleep(task, 1200);
}

bool demo_start_readers_writers(void)
{
    if (s_rw_started) return false;
    s_reader_count = 0;
    s_rw_value = 0;
    kmemset(s_reader_contexts, 0, sizeof(s_reader_contexts));
    kmemset(&s_writer_context, 0, sizeof(s_writer_context));
    s_reader_contexts[0].number = 1;
    s_reader_contexts[1].number = 2;
    semaphore_init(&s_reader_mutex, 1);
    semaphore_init(&s_rw_resource, 1);
    int r1 = task_create("reader-1", reader_step, &s_reader_contexts[0]);
    int r2 = task_create("reader-2", reader_step, &s_reader_contexts[1]);
    int w = task_create("writer", writer_step, &s_writer_context);
    if (r1 < 0 || r2 < 0 || w < 0) {
        if (r1 >= 0) task_kill((uint32_t)r1);
        if (r2 >= 0) task_kill((uint32_t)r2);
        if (w >= 0) task_kill((uint32_t)w);
        return false;
    }
    s_reader_ids[0] = (uint32_t)r1;
    s_reader_ids[1] = (uint32_t)r2;
    s_writer_id = (uint32_t)w;
    s_rw_started = true;
    event_log_add("SYNC", "readers-writers started (%u,%u,%u)",
                  s_reader_ids[0], s_reader_ids[1], s_writer_id);
    return true;
}

bool demo_stop_readers_writers(void)
{
    if (!s_rw_started) return false;
    task_kill(s_reader_ids[0]);
    task_kill(s_reader_ids[1]);
    task_kill(s_writer_id);
    s_rw_started = false;
    s_reader_count = 0;
    event_log_add("SYNC", "readers-writers stopped");
    return true;
}

void demo_get_rw_status(demo_rw_status_t *out)
{
    if (!out) return;
    out->started = s_rw_started;
    out->reader_ids[0] = s_reader_ids[0];
    out->reader_ids[1] = s_reader_ids[1];
    out->writer_id = s_writer_id;
    out->active_readers = s_reader_count;
    out->shared_value = s_rw_value;
}
