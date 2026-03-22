#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>

/* SPI HAL abstraction — function pointer struct.
 * On target: hal_spi_stm32.c binds to HAL_SPI_TransmitReceive.
 * In tests:  mock_spi.c records TX and returns controlled RX data. */

typedef struct {
    /* Full-duplex transfer of `len` bytes.
     * tx and rx must both be at least `len` bytes.
     * Returns 0 on success, non-zero on error. */
    int (*transfer)(uint8_t *tx, uint8_t *rx, uint16_t len);
} hal_spi_t;

#endif /* HAL_SPI_H */
