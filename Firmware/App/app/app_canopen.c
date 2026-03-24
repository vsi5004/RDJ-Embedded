#include "app_canopen.h"
#include "drv_tmc5160.h"
#include "hal_can.h"
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
static hal_can_t             *s_can;
static uint8_t                s_node_id;
static uint16_t               s_cfg_dmax;   /* configured DMAX; set via canopen_set_cfg_dmax() */

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

void canopen_init(void)
{
    memset(&s_rpdo, 0, sizeof(s_rpdo));
    memset(&s_tpdo, 0, sizeof(s_tpdo));
    s_status_word = 0;
    s_tof_mm      = 0;
    s_can         = NULL;
    s_node_id     = 0;
    s_cfg_dmax    = 0;
}

void canopen_set_cfg_dmax(uint16_t dmax)
{
    s_cfg_dmax = dmax;
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

        if (!(s_status_word & CANOPEN_STATUS_FAULT)) {
            if (s_rpdo.ctrl_word & CANOPEN_CTRL_ENABLE) {
                tmc5160_enable();
            }

            uint32_t vel = (s_rpdo.ctrl_word & CANOPEN_CTRL_HALT) ? 0u : s_rpdo.max_vel;

            /* Decode RPDO byte 7 bitfield:
             *   bits [1:0] = ramp_mode (0=pos, 1=vel+, 2=vel-)
             *   bits [7:2] = dmax_factor (0=use cfg DMAX; 1-63 = factor/64 * DMAX)
             */
            uint8_t  ramp_mode_val = s_rpdo.ramp_mode & 0x03U;
            uint8_t  dmax_factor   = (s_rpdo.ramp_mode >> 2U) & 0x3FU;

            /* Apply scaled DMAX in position mode only; skip during velocity mode */
            if (ramp_mode_val == TMC5160_RAMPMODE_POSITION && dmax_factor > 0U) {
                uint32_t dmax = (s_cfg_dmax > 0U)
                    ? ((uint32_t)s_cfg_dmax * dmax_factor) / 64U
                    : 0U;
                if (dmax < 1U) { dmax = 1U; }
                tmc5160_write(TMC5160_REG_DMAX, dmax);
            } else if (ramp_mode_val == TMC5160_RAMPMODE_POSITION) {
                /* dmax_factor == 0: restore configured DMAX */
                tmc5160_write(TMC5160_REG_DMAX, s_cfg_dmax);
            }

            tmc5160_set_rampmode(ramp_mode_val);
            tmc5160_set_velocity(vel);
            tmc5160_set_target(s_rpdo.target_pos);
        }

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

/* -------------------------------------------------------------------------
 * Fault / EMCY
 * ------------------------------------------------------------------------- */

void canopen_bind_can(hal_can_t *can, uint8_t node_id)
{
    s_can     = can;
    s_node_id = node_id;
}

void canopen_report_fault(uint16_t emcy_code)
{
    canopen_set_status_bit(CANOPEN_STATUS_FAULT);

    if (s_can) {
        uint8_t frame[8] = {
            (uint8_t)(emcy_code & 0xFFu),
            (uint8_t)(emcy_code >> 8),
            0x01u,  /* error register — bit 0: generic error */
            0, 0, 0, 0, 0,
        };
        s_can->transmit(0x80u + s_node_id, frame, 8);
    }
}
