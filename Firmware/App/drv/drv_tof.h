#ifndef DRV_TOF_H
#define DRV_TOF_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_i2c.h"

/* VL53L4CD / VL53L1X Time-of-Flight sensor driver (I2C, non-blocking).
 *
 * Usage pattern (call from super-loop):
 *   drv_tof_init(&hal_i2c1);
 *   drv_tof_start_ranging();
 *   ...
 *   if (drv_tof_is_data_ready()) {
 *       uint16_t mm = drv_tof_get_distance_mm();
 *       drv_tof_clear_interrupt();
 *       drv_tof_start_ranging();      // restart for next measurement
 *   }
 *
 * All VL53 registers use 16-bit addresses sent MSB-first over I2C.
 */

#define TOF_I2C_ADDR     0x29u  /* default 7-bit I2C address */

/* Register addresses */
#define TOF_REG_CLEAR_INT 0x0086u  /* SYSTEM__INTERRUPT_CLEAR: write 0x01 */
#define TOF_REG_START     0x0087u  /* SYSTEM__START: 0x40 = start single shot */
#define TOF_REG_STATUS    0x0031u  /* GPIO__TIO_HV_STATUS: bit 0 = 0 → data ready */
#define TOF_REG_RANGE_MM  0x0096u  /* RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0 (2 B) */

/* Bind the I2C peripheral and run minimal device configuration.
 * Must be called once before any other drv_tof_* function. */
void drv_tof_init(hal_i2c_t *i2c);

/* Trigger one ranging measurement. */
void drv_tof_start_ranging(void);

/* Returns true when a new distance result is available.
 * Polls TOF_REG_STATUS: ready when bit 0 == 0 (interrupt asserted). */
bool drv_tof_is_data_ready(void);

/* Read the most recent distance result (mm, 16-bit, big-endian on wire).
 * Call only when is_data_ready() returns true. */
uint16_t drv_tof_get_distance_mm(void);

/* Clear the data-ready interrupt flag so the next ranging result is visible. */
void drv_tof_clear_interrupt(void);

#endif /* DRV_TOF_H */
