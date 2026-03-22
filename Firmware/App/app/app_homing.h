#ifndef APP_HOMING_H
#define APP_HOMING_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Homing state machine — single axis, all three stepper nodes use this.
 *
 * The SBC is responsible for deciding when it is safe to home each axis
 * (geometry, slot clearance, A axis angle, etc.). The firmware just executes
 * the seek-and-zero sequence when commanded via app_homing_start().
 *
 * Sequence:
 *   1. start()  — set HOMING status bit, write TMC5160 to velocity mode
 *                 (VEL_NEG) at seek_velocity toward the endstop (position 0)
 *   2. tick()   — poll RAMPSTAT each super-loop iteration; TMC5160 stops
 *                 itself internally when the endstop fires (stop_l_enable)
 *   3. On EVENT_STOP_L — zero XACTUAL, restore position mode,
 *                 set HOMED bit, clear HOMING bit → back to IDLE
 * ------------------------------------------------------------------------- */

/* Bind the seek velocity. Must be called once before app_homing_start(). */
void app_homing_init(uint32_t seek_velocity);

/* Begin homing. Sets HOMING status bit and starts TMC5160 moving.
 * Ignored (no-op) if homing is already in progress. */
void app_homing_start(void);

/* Advance the state machine. Call every super-loop iteration.
 * No-op when idle. */
void app_homing_tick(void);

/* Returns true while a homing sequence is in progress. */
bool app_homing_is_active(void);

#endif /* APP_HOMING_H */
