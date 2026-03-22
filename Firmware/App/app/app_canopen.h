#ifndef APP_CANOPEN_H
#define APP_CANOPEN_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Status word bit masks (OD 0x2003)
 * ------------------------------------------------------------------------- */
#define CANOPEN_STATUS_HOMED             (1u << 0)
#define CANOPEN_STATUS_MOVING            (1u << 1)
#define CANOPEN_STATUS_IN_POSITION       (1u << 2)
#define CANOPEN_STATUS_FAULT             (1u << 3)
#define CANOPEN_STATUS_HOMING            (1u << 4)
#define CANOPEN_STATUS_STALLGUARD_ACTIVE (1u << 5)

/* -------------------------------------------------------------------------
 * Control word bit masks (OD 0x2004)
 * ------------------------------------------------------------------------- */
#define CANOPEN_CTRL_ENABLE              (1u << 0)
#define CANOPEN_CTRL_HOME                (1u << 1)
#define CANOPEN_CTRL_HALT                (1u << 2)
#define CANOPEN_CTRL_CLEAR_FAULT         (1u << 3)

/* -------------------------------------------------------------------------
 * TPDO snapshot — stepper node layout (latched on every SYNC)
 *
 * Byte layout matches TPDO1 for stepper nodes:
 *   bytes 0–3 : actual_position  (INT32, XACTUAL)
 *   byte  4   : status_word      (UINT8, OD 0x2003)
 *   byte  5   : ramp_status      (UINT8, RAMPSTAT[7:0])
 *   bytes 6–7 : tof_distance_mm  (UINT16, VL53 reading)
 * ------------------------------------------------------------------------- */
typedef struct {
    int32_t  actual_position;
    uint8_t  status_word;
    uint8_t  ramp_status;
    uint16_t tof_distance_mm;
} canopen_tpdo_stepper_t;

/* -------------------------------------------------------------------------
 * Lifecycle
 * Must be called after tmc5160_init() on stepper nodes.
 * ------------------------------------------------------------------------- */
void canopen_init(void);

/* -------------------------------------------------------------------------
 * RPDO interface
 * Call from the CANopenNode OD write callback when RPDO1 is received.
 * Values are buffered — NOT applied until canopen_sync_process().
 * ------------------------------------------------------------------------- */
void canopen_stepper_rpdo_write(int32_t  target_pos,
                                uint32_t max_vel,
                                uint8_t  ctrl_word,
                                uint8_t  ramp_mode);

/* -------------------------------------------------------------------------
 * SYNC interface
 * Call from the CANopenNode SYNC callback (fires from CAN RX interrupt via
 * CO_SYNC_initCallbackPre). Latches TPDO data then applies staged RPDO.
 * ------------------------------------------------------------------------- */
void canopen_sync_process(void);

/* -------------------------------------------------------------------------
 * Sensor injection
 * Push the latest ToF reading from the 10Hz sensor poll loop.
 * The value is latched into the TPDO on the next canopen_sync_process().
 * ------------------------------------------------------------------------- */
void canopen_update_tof(uint16_t mm);

/* -------------------------------------------------------------------------
 * Status word
 * ------------------------------------------------------------------------- */
uint8_t canopen_get_status_word(void);
void    canopen_set_status_bit  (uint8_t mask);
void    canopen_clear_status_bit(uint8_t mask);

/* -------------------------------------------------------------------------
 * TPDO readback
 * Returns a pointer to the snapshot latched by the most recent SYNC.
 * ------------------------------------------------------------------------- */
const canopen_tpdo_stepper_t *canopen_get_tpdo_stepper(void);

#endif /* APP_CANOPEN_H */
