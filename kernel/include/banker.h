#ifndef MINI_KERNEL_BANKER_H
#define MINI_KERNEL_BANKER_H

#include "ktypes.h"

#define BANKER_PROCESS_COUNT 5
#define BANKER_RESOURCE_COUNT 3

void banker_init_demo(void);
bool banker_is_safe(uint8_t safe_sequence[BANKER_PROCESS_COUNT]);
bool banker_request(uint8_t process_id,
                    const uint16_t request[BANKER_RESOURCE_COUNT],
                    uint8_t safe_sequence[BANKER_PROCESS_COUNT]);
bool banker_release(uint8_t process_id,
                    const uint16_t release[BANKER_RESOURCE_COUNT]);
void banker_get_available(uint16_t out[BANKER_RESOURCE_COUNT]);
void banker_get_maximum(uint8_t process, uint16_t out[BANKER_RESOURCE_COUNT]);
void banker_get_allocation(uint8_t process, uint16_t out[BANKER_RESOURCE_COUNT]);
void banker_get_need(uint8_t process, uint16_t out[BANKER_RESOURCE_COUNT]);

#endif
