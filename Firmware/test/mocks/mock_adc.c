#include "mock_adc.h"

uint32_t mock_adc_next_value = 0;
int      mock_adc_call_count = 0;

void mock_adc_reset(void)
{
    mock_adc_next_value = 0;
    mock_adc_call_count = 0;
}

static uint32_t mock_read(void)
{
    mock_adc_call_count++;
    return mock_adc_next_value;
}

hal_adc_t mock_hal_adc = { .read = mock_read };
