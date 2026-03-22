#include "app_canopen.h"
#include "drv_tmc5160.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------------- */

typedef struct {
    int32_t  target_pos;
    uint32_t max_vel;
    uint8_t  ctrl_word;
    uint8_t  ramp_mode;
    bool     pending;
} rpdo_shadow_t;

static rpdo_shadow_t          s_rpdo;
static canopen_tpdo_stepper_t s_tpdo;
static uint8_t                s_status_word;
static uint16_t               s_tof_mm;

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

void canopen_init(void)
{
    memset(&s_rpdo, 0, sizeof(s_rpdo));
    memset(&s_tpdo, 0, sizeof(s_tpdo));
    s_status_word = 0;
    s_tof_mm      = 0;
}

/* -------------------------------------------------------------------------
 * RPDO staging
 * Called from the CANopenNode OD write callback on RPDO receive.
 * All four fields are always updated together (one RPDO frame = one atomic set).
 * ------------------------------------------------------------------------- */

void canopen_stepper_rpdo_write(int32_t  target_pos,
                                uint32_t max_vel,
                                uint8_t  ctrl_word,
                                uint8_t  ramp_mode)
{
    s_rpdo.target_pos = target_pos;
    s_rpdo.max_vel    = max_vel;
    s_rpdo.ctrl_word  = ctrl_word;
    s_rpdo.ramp_mode  = ramp_mode;
    s_rpdo.pending    = true;
}

/* -------------------------------------------------------------------------
 * SYNC processing
 *
 * Order matches the super-loop design: latch TPDO first (captures state at
 * SYNC arrival), then apply queued RPDO (commands take effect next cycle).
 * This guarantees multi-axis SYNC-gated motion — all nodes latch their
 * current position BEFORE any of them start moving.
 * ------------------------------------------------------------------------- */

static void update_status_from_rampstat(uint32_t rampstat)
{
    if (rampstat & TMC5160_RAMPSTAT_VZERO) {
        s_status_word &= ~CANOPEN_STATUS_MOVING;
    } else {
        s_status_word |= CANOPEN_STATUS_MOVING;
    }

    if (rampstat & TMC5160_RAMPSTAT_POS_REACHED) {
        s_status_word |= CANOPEN_STATUS_IN_POSITION;
    } else {
        s_status_word &= ~CANOPEN_STATUS_IN_POSITION;
    }
}

void canopen_sync_process(void)
{
    /* 1. Read fresh status from TMC5160 and update status word */
    uint32_t rampstat = tmc5160_get_rampstat();
    update_status_from_rampstat(rampstat);

    /* 2. Latch TPDO snapshot — captures state at SYNC arrival */
    s_tpdo.actual_position = tmc5160_get_position();
    s_tpdo.status_word     = s_status_word;
    s_tpdo.ramp_status     = (uint8_t)(rampstat & 0xFFu);
    s_tpdo.tof_distance_mm = s_tof_mm;

    /* 3. Apply staged RPDO if one is waiting */
    if (s_rpdo.pending) {
        if (s_rpdo.ctrl_word & CANOPEN_CTRL_CLEAR_FAULT) {
            s_status_word &= ~CANOPEN_STATUS_FAULT;
        }
        if (s_rpdo.ctrl_word & CANOPEN_CTRL_ENABLE) {
            tmc5160_enable();
        }

        uint32_t vel = (s_rpdo.ctrl_word & CANOPEN_CTRL_HALT) ? 0u : s_rpdo.max_vel;

        tmc5160_set_rampmode(s_rpdo.ramp_mode);
        tmc5160_set_velocity(vel);
        tmc5160_set_target(s_rpdo.target_pos);

        s_rpdo.pending = false;
    }
}

/* -------------------------------------------------------------------------
 * Sensor injection
 * ------------------------------------------------------------------------- */

void canopen_update_tof(uint16_t mm)
{
    s_tof_mm = mm;
}

/* -------------------------------------------------------------------------
 * Status word
 * ------------------------------------------------------------------------- */

uint8_t canopen_get_status_word(void)
{
    return s_status_word;
}

void canopen_set_status_bit(uint8_t mask)
{
    s_status_word |= mask;
}

void canopen_clear_status_bit(uint8_t mask)
{
    s_status_word &= ~mask;
}

/* -------------------------------------------------------------------------
 * TPDO readback
 * ------------------------------------------------------------------------- */

const canopen_tpdo_stepper_t *canopen_get_tpdo_stepper(void)
{
    return &s_tpdo;
}
