#ifndef MOCK_PWM_H
#define MOCK_PWM_H

#include <stdint.h>
#include "hal_pwm.h"

extern uint8_t  mock_pwm_last_channel;
extern uint32_t mock_pwm_last_pulse_us;
extern int      mock_pwm_call_count;

void mock_pwm_reset(void);

extern hal_pwm_t mock_hal_pwm;

#endif /* MOCK_PWM_H */
