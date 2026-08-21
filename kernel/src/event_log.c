#include "event_log.h"
#include "klib.h"

static event_log_entry_t s_entries[EVENT_LOG_CAPACITY];
static size_t s_start;
static size_t s_count;
static uint32_t s_sequence;

void event_log_init(void)
{
    kmemset(s_entries, 0, sizeof(s_entries));
    s_start = 0;
    s_count = 0;
    s_sequence = 0;
}

void event_log_add(const char *category, const char *format, ...)
{
    if (category == NULL || format == NULL) return;
    size_t slot;
    if (s_count < EVENT_LOG_CAPACITY) {
        slot = (s_start + s_count) % EVENT_LOG_CAPACITY;
        ++s_count;
    } else {
        slot = s_start;
        s_start = (s_start + 1U) % EVENT_LOG_CAPACITY;
    }
    event_log_entry_t *entry = &s_entries[slot];
    kmemset(entry, 0, sizeof(*entry));
    entry->sequence = ++s_sequence;
    ksnprintf(entry->category, sizeof(entry->category), "%s", category);
    va_list arguments;
    va_start(arguments, format);
    kvsnprintf(entry->message, sizeof(entry->message), format, arguments);
    va_end(arguments);
}

void event_log_clear(void)
{
    s_start = 0;
    s_count = 0;
    kmemset(s_entries, 0, sizeof(s_entries));
}

size_t event_log_count(void) { return s_count; }

bool event_log_get(size_t newest_offset, event_log_entry_t *entry)
{
    if (entry == NULL || newest_offset >= s_count) return false;
    const size_t logical = s_count - 1U - newest_offset;
    *entry = s_entries[(s_start + logical) % EVENT_LOG_CAPACITY];
    return true;
}
