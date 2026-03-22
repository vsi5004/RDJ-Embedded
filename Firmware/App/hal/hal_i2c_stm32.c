#include "hal_i2c.h"
#include "i2c.h"   /* CubeMX-generated: extern I2C_HandleTypeDef hi2c1 */

#define I2C_TIMEOUT_MS 50

static int i2c1_write(uint8_t addr, uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef s = HAL_I2C_Master_Transmit(&hi2c1,
                                                   (uint16_t)(addr << 1),
                                                   data, len,
                                                   I2C_TIMEOUT_MS);
    return (s == HAL_OK) ? 0 : -1;
}

static int i2c1_read(uint8_t addr, uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef s = HAL_I2C_Master_Receive(&hi2c1,
                                                  (uint16_t)(addr << 1),
                                                  data, len,
                                                  I2C_TIMEOUT_MS);
    return (s == HAL_OK) ? 0 : -1;
}

static int i2c1_write_read(uint8_t addr,
                            uint8_t *tx, uint16_t tx_len,
                            uint8_t *rx, uint16_t rx_len)
{
    HAL_StatusTypeDef s;
    s = HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(addr << 1),
                                 tx, tx_len, I2C_TIMEOUT_MS);
    if (s != HAL_OK) return -1;
    s = HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(addr << 1),
                                rx, rx_len, I2C_TIMEOUT_MS);
    return (s == HAL_OK) ? 0 : -1;
}

hal_i2c_t hal_i2c1 = {
    .write      = i2c1_write,
    .read       = i2c1_read,
    .write_read = i2c1_write_read,
};
