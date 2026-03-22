#ifndef DRV_SERVO_H
#define DRV_SERVO_H

#include <stdint.h>
#include "hal_pwm.h"

/* RC servo PWM driver — TIM3 CH1/CH2 on the Pincher and Player nodes.
 *
 * Servo channels: 1 = grip/play, 2 = flip/speed (matches TIM3 CH1/CH2).
 * Pulse range: 500–2500 μs (standard RC servo).
 * Centre: 1500 μs.  Full throw: ±1000 μs from centre (~90° per 1000 μs).
 *
 * drv_servo_set() clamps the requested pulse width to [SERVO_PULSE_MIN_US,
 * SERVO_PULSE_MAX_US] before writing the HAL — out-of-range values are
 * silently clamped rather than rejected, matching common servo library
 * behaviour and protecting against SBC command errors.
 */

#define SERVO_PULSE_MIN_US  500u
#define SERVO_PULSE_MAX_US  2500u

/* Bind the PWM HAL instance. Must be called before drv_servo_set(). */
void drv_servo_init(hal_pwm_t *pwm);

/* Set servo pulse width.
 * channel : 1 or 2
 * pulse_us: requested pulse width — clamped to [500, 2500] μs */
void drv_servo_set(uint8_t channel, uint32_t pulse_us);

#endif /* DRV_SERVO_H */
