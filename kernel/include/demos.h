#ifndef MINI_KERNEL_DEMOS_H
#define MINI_KERNEL_DEMOS_H

#include "ktypes.h"

#define DEMO_BUFFER_SIZE 4
#define DEMO_LOG_CAP 12

typedef struct {
    bool started;
    bool paused;
    uint32_t producer_id;
    uint32_t consumer_id;
    size_t buffer_count;
    int buffer[DEMO_BUFFER_SIZE];
    int empty_slots;
    int full_slots;
    int mutex;
} demo_pc_status_t;

typedef struct {
    bool started;
    uint32_t worker_a_id;
    uint32_t worker_b_id;
    uint32_t shared_counter;
} demo_thread_status_t;

typedef struct {
    bool started;
    uint32_t reader_ids[2];
    uint32_t writer_id;
    uint32_t active_readers;
    uint32_t shared_value;
} demo_rw_status_t;

int demo_create_counter(void);
int demo_create_counter_named(const char *name);
bool demo_start_producer_consumer(void);
bool demo_pause_producer_consumer(void);
bool demo_resume_producer_consumer(void);
bool demo_stop_producer_consumer(void);
void demo_get_pc_status(demo_pc_status_t *out);
size_t demo_log_count(void);
const char *demo_log_get(size_t newest_offset);
bool demo_start_threads(void);
bool demo_stop_threads(void);
void demo_get_thread_status(demo_thread_status_t *out);
bool demo_start_readers_writers(void);
bool demo_stop_readers_writers(void);
void demo_get_rw_status(demo_rw_status_t *out);

#endif
