#ifndef MOCK_CAN_H
#define MOCK_CAN_H

#include <stdint.h>
#include "hal_can.h"

#define MOCK_CAN_MAX_FRAMES 16

typedef struct {
    uint32_t id;
    uint8_t  data[8];
    uint8_t  len;
} mock_can_frame_t;

extern mock_can_frame_t mock_can_tx_log[MOCK_CAN_MAX_FRAMES];
extern int              mock_can_tx_count;

void mock_can_reset(void);

extern hal_can_t mock_hal_can;

#endif /* MOCK_CAN_H */
