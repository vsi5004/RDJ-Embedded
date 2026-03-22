#include "mock_i2c.h"
#include <string.h>

mock_i2c_record_t mock_i2c_log[MOCK_I2C_MAX_TRANSFERS];
int               mock_i2c_call_count = 0;

static uint8_t  s_rx_buf[MOCK_I2C_MAX_TRANSFERS][MOCK_I2C_MAX_BYTES];
static uint16_t s_rx_len[MOCK_I2C_MAX_TRANSFERS];

void mock_i2c_reset(void)
{
    memset(mock_i2c_log, 0, sizeof(mock_i2c_log));
    memset(s_rx_buf,     0, sizeof(s_rx_buf));
    memset(s_rx_len,     0, sizeof(s_rx_len));
    mock_i2c_call_count = 0;
}

void mock_i2c_program_read_response(int idx, const uint8_t *data, uint16_t len)
{
    if (idx < 0 || idx >= MOCK_I2C_MAX_TRANSFERS) return;
    uint16_t copy = (len > MOCK_I2C_MAX_BYTES) ? MOCK_I2C_MAX_BYTES : len;
    memcpy(s_rx_buf[idx], data, copy);
    s_rx_len[idx] = copy;
}

static int mock_write(uint8_t addr, uint8_t *data, uint16_t len)
{
    int idx = mock_i2c_call_count;
    if (idx >= MOCK_I2C_MAX_TRANSFERS) return -1;
    mock_i2c_log[idx].addr    = addr;
    mock_i2c_log[idx].is_read = 0;
    mock_i2c_log[idx].len     = len;
    uint16_t copy = (len > MOCK_I2C_MAX_BYTES) ? MOCK_I2C_MAX_BYTES : len;
    memcpy(mock_i2c_log[idx].data, data, copy);
    mock_i2c_call_count++;
    return 0;
}

static int mock_read(uint8_t addr, uint8_t *data, uint16_t len)
{
    int idx = mock_i2c_call_count;
    if (idx >= MOCK_I2C_MAX_TRANSFERS) return -1;
    mock_i2c_log[idx].addr    = addr;
    mock_i2c_log[idx].is_read = 1;
    mock_i2c_log[idx].len     = len;
    memset(data, 0, len);
    if (s_rx_len[idx] > 0) {
        uint16_t copy = (s_rx_len[idx] < len) ? s_rx_len[idx] : len;
        memcpy(data, s_rx_buf[idx], copy);
    }
    mock_i2c_call_count++;
    return 0;
}

static int mock_write_read(uint8_t addr,
                            uint8_t *tx, uint16_t tx_len,
                            uint8_t *rx, uint16_t rx_len)
{
    mock_write(addr, tx, tx_len);
    return mock_read(addr, rx, rx_len);
}

hal_i2c_t mock_hal_i2c = {
    .write      = mock_write,
    .read       = mock_read,
    .write_read = mock_write_read,
};
