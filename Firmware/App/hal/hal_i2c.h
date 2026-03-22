#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>

/* I2C HAL abstraction.
 * addr is the 7-bit device address (not shifted).
 * Returns 0 on success, non-zero on error. */

typedef struct {
    int (*write)(uint8_t addr, uint8_t *data, uint16_t len);
    int (*read) (uint8_t addr, uint8_t *data, uint16_t len);
    /* Write then repeated-start read (register read pattern). */
    int (*write_read)(uint8_t addr,
                      uint8_t *tx, uint16_t tx_len,
                      uint8_t *rx, uint16_t rx_len);
} hal_i2c_t;

#endif /* HAL_I2C_H */
