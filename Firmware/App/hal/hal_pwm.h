#ifndef HAL_PWM_H
#define HAL_PWM_H

#include <stdint.h>

/* PWM HAL abstraction — RC servo pulse width control.
 * channel: 1 or 2 (maps to TIM3 CH1/CH2).
 * pulse_us: pulse width in microseconds (500–2500 for RC servos). */

typedef struct {
    void (*set_pulse)(uint8_t channel, uint32_t pulse_us);
} hal_pwm_t;

#endif /* HAL_PWM_H */
