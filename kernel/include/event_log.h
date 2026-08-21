#ifndef MINI_KERNEL_EVENT_LOG_H
#define MINI_KERNEL_EVENT_LOG_H

#include "ktypes.h"

#define EVENT_LOG_CAPACITY 32U

typedef struct {
    uint32_t sequence;
    char category[12];
    char message[72];
} event_log_entry_t;

void event_log_init(void);
void event_log_add(const char *category, const char *format, ...);
void event_log_clear(void);
size_t event_log_count(void);
bool event_log_get(size_t newest_offset, event_log_entry_t *entry);

#endif
