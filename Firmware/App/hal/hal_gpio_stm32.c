#include "hal_gpio.h"
#include "gpio.h"
#include "main.h"

/* Pin IDs used by application code.
 * Defined here so drivers don't need to include main.h directly. */
#define GPIO_PIN_SPI1_CS   0
#define GPIO_PIN_STATUS_LED 1

static int gpio_read(uint8_t pin)
{
    switch (pin) {
    case GPIO_PIN_SPI1_CS:
        return HAL_GPIO_ReadPin(SPI1_CS_GPIO_Port, SPI1_CS_Pin);
    default:
        return 0;
    }
}

static void gpio_write(uint8_t pin, int state)
{
    GPIO_PinState s = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    switch (pin) {
    case GPIO_PIN_SPI1_CS:
        HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, s);
        break;
    case GPIO_PIN_STATUS_LED:
        HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, s);
        break;
    default:
        break;
    }
}

hal_gpio_t hal_gpio = { .read = gpio_read, .write = gpio_write };
