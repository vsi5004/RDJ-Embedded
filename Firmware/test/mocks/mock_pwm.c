#include "mock_pwm.h"

uint8_t  mock_pwm_last_channel  = 0;
uint32_t mock_pwm_last_pulse_us = 0;
int      mock_pwm_call_count    = 0;

void mock_pwm_reset(void)
{
    mock_pwm_last_channel  = 0;
    mock_pwm_last_pulse_us = 0;
    mock_pwm_call_count    = 0;
}

static void mock_set_pulse(uint8_t channel, uint32_t pulse_us)
{
    mock_pwm_last_channel  = channel;
    mock_pwm_last_pulse_us = pulse_us;
    mock_pwm_call_count++;
}

hal_pwm_t mock_hal_pwm = { .set_pulse = mock_set_pulse };
