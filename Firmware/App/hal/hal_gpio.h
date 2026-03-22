#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

/* GPIO HAL abstraction.
 * pin identifiers are defined by each driver that uses this interface.
 * read() returns 0 or 1.  write() sets the pin high (1) or low (0). */

typedef struct {
    int  (*read) (uint8_t pin);
    void (*write)(uint8_t pin, int state);
} hal_gpio_t;

#endif /* HAL_GPIO_H */
