#include "hal_spi.h"
#include "spi.h"   /* CubeMX-generated: extern SPI_HandleTypeDef hspi1, hspi2 */

/* ---------- SPI1 (TMC5160) ---------- */

static int spi1_transfer(uint8_t *tx, uint8_t *rx, uint16_t len)
{
    HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&hspi1, tx, rx, len, 10);
    return (s == HAL_OK) ? 0 : -1;
}

hal_spi_t hal_spi1 = { .transfer = spi1_transfer };

/* ---------- SPI2 (DotStar LEDs) ---------- */

static int spi2_transfer(uint8_t *tx, uint8_t *rx, uint16_t len)
{
    /* DotStar is write-only; rx may be NULL — use a scratch buffer. */
    uint8_t scratch[4];
    if (!rx) rx = scratch;
    HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&hspi2, tx, rx, len, 10);
    return (s == HAL_OK) ? 0 : -1;
}

hal_spi_t hal_spi2 = { .transfer = spi2_transfer };
