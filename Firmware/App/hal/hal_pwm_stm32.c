#include "hal_pwm.h"
#include "tim.h"   /* CubeMX-generated: extern TIM_HandleTypeDef htim3 */

/* TIM3 is configured at 1MHz tick (1μs resolution), period = 20000 (20ms = 50Hz).
 * CCR value equals pulse width in microseconds directly. */

static void pwm_set_pulse(uint8_t channel, uint32_t pulse_us)
{
    switch (channel) {
    case 1: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse_us); break;
    case 2: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pulse_us); break;
    default: break;
    }
}

hal_pwm_t hal_pwm_servo = { .set_pulse = pwm_set_pulse };
