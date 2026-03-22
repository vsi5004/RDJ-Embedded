#include "mock_gpio.h"
#include <string.h>

int mock_gpio_pin_state[MOCK_GPIO_MAX_PINS];
int mock_gpio_write_count = 0;

void mock_gpio_reset(void)
{
    memset(mock_gpio_pin_state, 0, sizeof(mock_gpio_pin_state));
    mock_gpio_write_count = 0;
}

static int mock_read(uint8_t pin)
{
    if (pin >= MOCK_GPIO_MAX_PINS) return 0;
    return mock_gpio_pin_state[pin];
}

static void mock_write(uint8_t pin, int state)
{
    if (pin >= MOCK_GPIO_MAX_PINS) return;
    mock_gpio_pin_state[pin] = state;
    mock_gpio_write_count++;
}

hal_gpio_t mock_hal_gpio = { .read = mock_read, .write = mock_write };
