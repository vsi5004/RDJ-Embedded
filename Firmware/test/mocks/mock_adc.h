#ifndef MOCK_ADC_H
#define MOCK_ADC_H

#include <stdint.h>
#include "hal_adc.h"

extern uint32_t mock_adc_next_value;
extern int      mock_adc_call_count;

void mock_adc_reset(void);

extern hal_adc_t mock_hal_adc;

#endif /* MOCK_ADC_H */
