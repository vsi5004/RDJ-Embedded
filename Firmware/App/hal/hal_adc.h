#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdint.h>

/* ADC HAL abstraction.
 * read() triggers a conversion and returns the raw result.
 * With 16x hardware oversampling on STM32G4 this is a 16-bit value. */

typedef struct {
    uint32_t (*read)(void);
} hal_adc_t;

#endif /* HAL_ADC_H */
