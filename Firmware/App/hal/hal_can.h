#ifndef HAL_CAN_H
#define HAL_CAN_H

#include <stdint.h>

/* CAN HAL abstraction.
 * CANopenNode drives CAN directly via its STM32 driver; this interface
 * is provided for any application code that needs to send raw frames
 * outside the CANopen stack (e.g. diagnostics). */

typedef struct {
    /* Transmit a CAN 2.0B standard frame.
     * id:    11-bit CAN ID
     * data:  payload (up to 8 bytes)
     * len:   number of data bytes (0–8)
     * Returns 0 on success, non-zero if TX mailbox full. */
    int (*transmit)(uint32_t id, uint8_t *data, uint8_t len);
} hal_can_t;

#endif /* HAL_CAN_H */
