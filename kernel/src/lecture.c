#include "lecture.h"
#include "event_log.h"
#include "klib.h"

#define REPLACE_MAX_FRAMES 8
#define IPC_CAPACITY 96

static char s_mailbox[IPC_CAPACITY];
static bool s_mailbox_full;
static uint32_t s_sent, s_received;

void lecture_init(void)
{
    s_mailbox[0] = 0;
    s_mailbox_full = false;
    s_sent = s_received = 0;
}

static bool contains(const int *frames, size_t count, int page)
{
    for (size_t i = 0; i < count; ++i) if (frames[i] == page) return true;
    return false;
}

static uint32_t fifo_faults(const uint8_t *refs, size_t count, size_t frame_count)
{
    int frames[REPLACE_MAX_FRAMES];
    for (size_t i = 0; i < frame_count; ++i) frames[i] = -1;
    size_t next = 0;
    uint32_t faults = 0;
    for (size_t i = 0; i < count; ++i) {
        if (contains(frames, frame_count, refs[i])) continue;
        ++faults;
        frames[next] = refs[i];
        next = (next + 1U) % frame_count;
    }
    return faults;
}

static uint32_t lru_faults(const uint8_t *refs, size_t count, size_t frame_count)
{
    int frames[REPLACE_MAX_FRAMES];
    uint32_t used[REPLACE_MAX_FRAMES];
    for (size_t i = 0; i < frame_count; ++i) { frames[i] = -1; used[i] = 0; }
    uint32_t faults = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t slot = frame_count;
        for (size_t f = 0; f < frame_count; ++f) {
            if (frames[f] == refs[i]) { slot = f; break; }
        }
        if (slot == frame_count) {
            ++faults;
            slot = 0;
            for (size_t f = 0; f < frame_count; ++f) {
                if (frames[f] < 0) { slot = f; break; }
                if (used[f] < used[slot]) slot = f;
            }
            frames[slot] = refs[i];
        }
        used[slot] = (uint32_t)i + 1U;
    }
    return faults;
}

static uint32_t optimal_faults(const uint8_t *refs, size_t count, size_t frame_count)
{
    int frames[REPLACE_MAX_FRAMES];
    for (size_t i = 0; i < frame_count; ++i) frames[i] = -1;
    uint32_t faults = 0;
    for (size_t i = 0; i < count; ++i) {
        if (contains(frames, frame_count, refs[i])) continue;
        ++faults;
        size_t victim = 0;
        bool placed = false;
        size_t farthest = 0;
        for (size_t f = 0; f < frame_count; ++f) {
            if (frames[f] < 0) { victim = f; placed = true; break; }
            size_t next = count;
            for (size_t j = i + 1; j < count; ++j) {
                if (refs[j] == (uint8_t)frames[f]) { next = j; break; }
            }
            if (f == 0 || next > farthest) { farthest = next; victim = f; }
        }
        (void)placed;
        frames[victim] = refs[i];
    }
    return faults;
}

replacement_result_t replacement_compare(const uint8_t *references, size_t count,
                                         size_t frame_count)
{
    replacement_result_t result = {0, 0, 0};
    if (!references || !count || frame_count == 0 || frame_count > REPLACE_MAX_FRAMES)
        return result;
    result.fifo_faults = fifo_faults(references, count, frame_count);
    result.lru_faults = lru_faults(references, count, frame_count);
    result.optimal_faults = optimal_faults(references, count, frame_count);
    return result;
}

fit_result_t fit_compare(const uint16_t *holes, size_t count, uint16_t request)
{
    fit_result_t result = {-1, -1, -1};
    if (!holes || request == 0) return result;
    for (size_t i = 0; i < count; ++i) {
        if (holes[i] < request) continue;
        if (result.first_fit < 0) result.first_fit = (int)i;
        if (result.best_fit < 0 || holes[i] < holes[result.best_fit]) result.best_fit = (int)i;
        if (result.worst_fit < 0 || holes[i] > holes[result.worst_fit]) result.worst_fit = (int)i;
    }
    return result;
}

bool ipc_send(const char *message)
{
    if (!message || !message[0] || s_mailbox_full || kstrlen(message) >= IPC_CAPACITY) return false;
    ksnprintf(s_mailbox, sizeof(s_mailbox), "%s", message);
    s_mailbox_full = true;
    ++s_sent;
    event_log_add("IPC", "mailbox send (%u B)", (unsigned)kstrlen(message));
    return true;
}

bool ipc_receive(char *output, size_t capacity)
{
    if (!output || capacity == 0 || !s_mailbox_full) return false;
    ksnprintf(output, capacity, "%s", s_mailbox);
    s_mailbox[0] = 0;
    s_mailbox_full = false;
    ++s_received;
    event_log_add("IPC", "mailbox receive");
    return true;
}

bool ipc_has_message(void) { return s_mailbox_full; }
uint32_t ipc_sent_count(void) { return s_sent; }
uint32_t ipc_received_count(void) { return s_received; }
