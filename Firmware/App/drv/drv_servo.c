#include "drv_servo.h"

static hal_pwm_t *s_pwm;

void drv_servo_init(hal_pwm_t *pwm)
{
    s_pwm = pwm;
}

void drv_servo_set(uint8_t channel, uint32_t pulse_us)
{
    if (pulse_us < SERVO_PULSE_MIN_US) {
        pulse_us = SERVO_PULSE_MIN_US;
    } else if (pulse_us > SERVO_PULSE_MAX_US) {
        pulse_us = SERVO_PULSE_MAX_US;
    }

    s_pwm->set_pulse(channel, pulse_us);
}
