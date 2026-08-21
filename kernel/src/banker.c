#include "banker.h"
#include "klib.h"

typedef struct {
    uint16_t available[BANKER_RESOURCE_COUNT];
    uint16_t maximum[BANKER_PROCESS_COUNT][BANKER_RESOURCE_COUNT];
    uint16_t allocation[BANKER_PROCESS_COUNT][BANKER_RESOURCE_COUNT];
} banker_state_t;

static banker_state_t s_state;

void banker_init_demo(void)
{
    const banker_state_t initial = {
        .available = {3, 3, 2},
        .maximum = {
            {7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}
        },
        .allocation = {
            {0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}
        }
    };
    s_state = initial;
}

static uint16_t need(uint8_t process, uint8_t resource)
{
    return s_state.maximum[process][resource] - s_state.allocation[process][resource];
}

bool banker_is_safe(uint8_t safe_sequence[BANKER_PROCESS_COUNT])
{
    uint16_t work[BANKER_RESOURCE_COUNT];
    bool finished[BANKER_PROCESS_COUNT] = {false};
    size_t completed = 0;
    kmemcpy(work, s_state.available, sizeof(work));

    while (completed < BANKER_PROCESS_COUNT) {
        bool found = false;
        for (uint8_t process = 0; process < BANKER_PROCESS_COUNT; ++process) {
            if (finished[process]) continue;
            bool can_finish = true;
            for (uint8_t resource = 0; resource < BANKER_RESOURCE_COUNT; ++resource) {
                if (need(process, resource) > work[resource]) {
                    can_finish = false;
                    break;
                }
            }
            if (!can_finish) continue;
            for (uint8_t resource = 0; resource < BANKER_RESOURCE_COUNT; ++resource) {
                work[resource] += s_state.allocation[process][resource];
            }
            finished[process] = true;
            safe_sequence[completed++] = process;
            found = true;
        }
        if (!found) return false;
    }
    return true;
}

bool banker_request(uint8_t process_id, const uint16_t request[BANKER_RESOURCE_COUNT],
                    uint8_t safe_sequence[BANKER_PROCESS_COUNT])
{
    if (process_id >= BANKER_PROCESS_COUNT) return false;
    for (uint8_t resource = 0; resource < BANKER_RESOURCE_COUNT; ++resource) {
        if (request[resource] > need(process_id, resource) ||
            request[resource] > s_state.available[resource]) return false;
    }
    for (uint8_t resource = 0; resource < BANKER_RESOURCE_COUNT; ++resource) {
        s_state.available[resource] -= request[resource];
        s_state.allocation[process_id][resource] += request[resource];
    }
    if (banker_is_safe(safe_sequence)) return true;
    for (uint8_t resource = 0; resource < BANKER_RESOURCE_COUNT; ++resource) {
        s_state.available[resource] += request[resource];
        s_state.allocation[process_id][resource] -= request[resource];
    }
    return false;
}

bool banker_release(uint8_t process_id, const uint16_t release[BANKER_RESOURCE_COUNT])
{
    if (process_id >= BANKER_PROCESS_COUNT) return false;
    for (uint8_t resource = 0; resource < BANKER_RESOURCE_COUNT; ++resource) {
        if (release[resource] > s_state.allocation[process_id][resource]) return false;
    }
    for (uint8_t resource = 0; resource < BANKER_RESOURCE_COUNT; ++resource) {
        s_state.allocation[process_id][resource] -= release[resource];
        s_state.available[resource] += release[resource];
    }
    return true;
}

void banker_get_available(uint16_t out[BANKER_RESOURCE_COUNT])
{
    kmemcpy(out, s_state.available, sizeof(s_state.available));
}

void banker_get_maximum(uint8_t process, uint16_t out[BANKER_RESOURCE_COUNT])
{
    if (process >= BANKER_PROCESS_COUNT) return;
    kmemcpy(out, s_state.maximum[process], sizeof(s_state.maximum[process]));
}

void banker_get_allocation(uint8_t process, uint16_t out[BANKER_RESOURCE_COUNT])
{
    if (process >= BANKER_PROCESS_COUNT) return;
    kmemcpy(out, s_state.allocation[process], sizeof(s_state.allocation[process]));
}

void banker_get_need(uint8_t process, uint16_t out[BANKER_RESOURCE_COUNT])
{
    if (process >= BANKER_PROCESS_COUNT) return;
    for (uint8_t r = 0; r < BANKER_RESOURCE_COUNT; r++) out[r] = need(process, r);
}
