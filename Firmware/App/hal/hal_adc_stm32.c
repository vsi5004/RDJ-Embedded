#include "hal_adc.h"
#include "adc.h"   /* CubeMX-generated: extern ADC_HandleTypeDef hadc1 */

static uint32_t adc1_read(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}

hal_adc_t hal_adc1 = { .read = adc1_read };
