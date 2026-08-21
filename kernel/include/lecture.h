#ifndef MINI_KERNEL_LECTURE_H
#define MINI_KERNEL_LECTURE_H

#include "ktypes.h"

typedef struct {
    uint32_t fifo_faults;
    uint32_t lru_faults;
    uint32_t optimal_faults;
} replacement_result_t;

typedef struct {
    int first_fit;
    int best_fit;
    int worst_fit;
} fit_result_t;

void lecture_init(void);
replacement_result_t replacement_compare(const uint8_t *references, size_t count,
                                         size_t frame_count);
fit_result_t fit_compare(const uint16_t *holes, size_t count, uint16_t request);
bool ipc_send(const char *message);
bool ipc_receive(char *output, size_t capacity);
bool ipc_has_message(void);
uint32_t ipc_sent_count(void);
uint32_t ipc_received_count(void);

#endif
