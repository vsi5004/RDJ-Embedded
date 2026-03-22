#include "app_homing.h"
#include "app_canopen.h"
#include "drv_tmc5160.h"

/* -------------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------------- */

typedef enum {
    HOMING_IDLE,
    HOMING_SEEKING,
} homing_state_t;

static homing_state_t s_state;
static uint32_t       s_seek_velocity;

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

void app_homing_init(uint32_t seek_velocity)
{
    s_seek_velocity = seek_velocity;
    s_state         = HOMING_IDLE;
}

/* -------------------------------------------------------------------------
 * start()
 *
 * Called from the main loop when the SBC sets CTRL_HOME in the control word.
 * Writes the TMC5160 immediately — this executes in main-loop context, so
 * SPI access is safe.
 * ------------------------------------------------------------------------- */

void app_homing_start(void)
{
    if (s_state != HOMING_IDLE) {
        return;  /* already homing — ignore */
    }

    canopen_clear_status_bit(CANOPEN_STATUS_HOMED);
    canopen_set_status_bit(CANOPEN_STATUS_HOMING);

    tmc5160_set_rampmode(TMC5160_RAMPMODE_VEL_NEG);
    tmc5160_set_velocity(s_seek_velocity);

    s_state = HOMING_SEEKING;
}

/* -------------------------------------------------------------------------
 * tick()
 *
 * Advances the state machine. Call every super-loop iteration.
 * In SEEKING: polls RAMPSTAT. The TMC5160 stops itself when the endstop
 * fires (stop_l_enable in SWMODE). We detect this via EVENT_STOP_L.
 * ------------------------------------------------------------------------- */

void app_homing_tick(void)
{
    if (s_state != HOMING_SEEKING) {
        return;
    }

    uint32_t rampstat = tmc5160_get_rampstat();

    if (rampstat & TMC5160_RAMPSTAT_EVENT_STOP_L) {
        /* Endstop hit — zero position, restore position mode, update status */
        tmc5160_write(TMC5160_REG_XACTUAL, 0);
        tmc5160_set_rampmode(TMC5160_RAMPMODE_POSITION);

        canopen_clear_status_bit(CANOPEN_STATUS_HOMING);
        canopen_set_status_bit(CANOPEN_STATUS_HOMED);

        s_state = HOMING_IDLE;
    }
}

/* -------------------------------------------------------------------------
 * is_active()
 * ------------------------------------------------------------------------- */

bool app_homing_is_active(void)
{
    return s_state != HOMING_IDLE;
}
