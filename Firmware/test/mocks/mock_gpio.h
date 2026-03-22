#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#include <stdint.h>
#include "hal_gpio.h"

#define MOCK_GPIO_MAX_PINS 16

extern int mock_gpio_pin_state[MOCK_GPIO_MAX_PINS];  /* readable state */
extern int mock_gpio_write_count;

void mock_gpio_reset(void);

extern hal_gpio_t mock_hal_gpio;

#endif /* MOCK_GPIO_H */
