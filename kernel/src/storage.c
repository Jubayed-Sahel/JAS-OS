#include "storage.h"
#include "event_log.h"
#include "klib.h"

static io_stats_t s_io;

static uint32_t distance(uint16_t a, uint16_t b)
{
    return a > b ? (uint32_t)(a - b) : (uint32_t)(b - a);
}

static void append_move(disk_result_t *result, uint16_t *head, uint16_t next)
{
    result->head_movement += distance(*head, next);
    result->order[result->count++] = next;
    *head = next;
}

static void sort_ascending(uint16_t *values, size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        uint16_t value = values[i];
        size_t j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
}

void storage_init(void)
{
    kmemset(&s_io, 0, sizeof(s_io));
}

const char *disk_policy_name(disk_policy_t policy)
{
    switch (policy) {
        case DISK_FCFS: return "FCFS";
        case DISK_SSTF: return "SSTF";
        case DISK_SCAN: return "SCAN";
        case DISK_CSCAN: return "C-SCAN";
        case DISK_CLOOK: return "C-LOOK";
        default: return "UNKNOWN";
    }
}

bool disk_schedule(disk_policy_t policy, const uint16_t *requests, size_t count,
                   uint16_t head, bool direction_up, disk_result_t *result)
{
    if (!requests || !result || count == 0 || count > DISK_MAX_REQUESTS ||
        head > DISK_MAX_CYLINDER) return false;
    for (size_t i = 0; i < count; ++i) {
        if (requests[i] > DISK_MAX_CYLINDER) return false;
    }

    kmemset(result, 0, sizeof(*result));
    if (policy == DISK_FCFS) {
        for (size_t i = 0; i < count; ++i) append_move(result, &head, requests[i]);
        return true;
    }

    if (policy == DISK_SSTF) {
        bool used[DISK_MAX_REQUESTS];
        kmemset(used, 0, sizeof(used));
        for (size_t served = 0; served < count; ++served) {
            size_t best = count;
            uint32_t best_distance = 0xFFFFFFFFu;
            for (size_t i = 0; i < count; ++i) {
                uint32_t d = distance(head, requests[i]);
                if (!used[i] && d < best_distance) {
                    best = i;
                    best_distance = d;
                }
            }
            used[best] = true;
            append_move(result, &head, requests[best]);
        }
        return true;
    }

    uint16_t sorted[DISK_MAX_REQUESTS];
    kmemcpy(sorted, requests, count * sizeof(sorted[0]));
    sort_ascending(sorted, count);
    size_t split = 0;
    while (split < count && sorted[split] < head) ++split;

    if (policy == DISK_SCAN) {
        if (direction_up) {
            for (size_t i = split; i < count; ++i) append_move(result, &head, sorted[i]);
            if (head != DISK_MAX_CYLINDER) append_move(result, &head, DISK_MAX_CYLINDER);
            for (size_t i = split; i > 0; --i) append_move(result, &head, sorted[i - 1]);
        } else {
            for (size_t i = split; i > 0; --i) append_move(result, &head, sorted[i - 1]);
            if (head != 0) append_move(result, &head, 0);
            for (size_t i = split; i < count; ++i) append_move(result, &head, sorted[i]);
        }
        return true;
    }

    if (direction_up) {
        for (size_t i = split; i < count; ++i) append_move(result, &head, sorted[i]);
        if (split > 0) {
            if (policy == DISK_CSCAN) {
                if (head != DISK_MAX_CYLINDER) append_move(result, &head, DISK_MAX_CYLINDER);
                append_move(result, &head, 0);
            } else append_move(result, &head, sorted[0]);
            for (size_t i = policy == DISK_CLOOK ? 1U : 0U; i < split; ++i)
                append_move(result, &head, sorted[i]);
        }
    } else {
        for (size_t i = split; i > 0; --i) append_move(result, &head, sorted[i - 1]);
        if (split < count) {
            if (policy == DISK_CSCAN) {
                if (head != 0) append_move(result, &head, 0);
                append_move(result, &head, DISK_MAX_CYLINDER);
            } else append_move(result, &head, sorted[count - 1]);
            size_t start = policy == DISK_CLOOK ? count - 1U : count;
            while (start > split) {
                --start;
                append_move(result, &head, sorted[start]);
            }
        }
    }
    return true;
}

void io_simulate(io_mode_t mode, uint32_t bytes)
{
    if (bytes == 0) return;
    ++s_io.requests;
    s_io.bytes += bytes;
    if (mode == IO_POLLING) {
        s_io.polls += (bytes + 63U) / 64U;
    } else if (mode == IO_INTERRUPT) {
        s_io.interrupts += (bytes + 511U) / 512U;
    } else {
        ++s_io.dma_transfers;
        ++s_io.interrupts; /* completion interrupt */
    }
    event_log_add("IO", "%s transfer %u B",
                  mode == IO_POLLING ? "polling" : mode == IO_INTERRUPT ? "interrupt" : "DMA",
                  bytes);
}

io_stats_t io_get_stats(void) { return s_io; }

void io_reset_stats(void)
{
    kmemset(&s_io, 0, sizeof(s_io));
    event_log_add("IO", "I/O statistics reset");
}
