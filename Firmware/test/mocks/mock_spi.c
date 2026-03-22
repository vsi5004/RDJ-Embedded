#include "mock_spi.h"
#include <string.h>

mock_spi_record_t mock_spi_log[MOCK_SPI_MAX_TRANSFERS];
int               mock_spi_call_count = 0;

static uint8_t  s_rx_buf[MOCK_SPI_MAX_TRANSFERS][MOCK_SPI_MAX_BYTES];
static uint16_t s_rx_len[MOCK_SPI_MAX_TRANSFERS];

void mock_spi_reset(void)
{
    memset(mock_spi_log, 0, sizeof(mock_spi_log));
    memset(s_rx_buf,     0, sizeof(s_rx_buf));
    memset(s_rx_len,     0, sizeof(s_rx_len));
    mock_spi_call_count = 0;
}

void mock_spi_program_response(int idx, const uint8_t *rx, uint16_t len)
{
    if (idx < 0 || idx >= MOCK_SPI_MAX_TRANSFERS) return;
    uint16_t copy = (len > MOCK_SPI_MAX_BYTES) ? MOCK_SPI_MAX_BYTES : len;
    memcpy(s_rx_buf[idx], rx, copy);
    s_rx_len[idx] = copy;
}

static int mock_transfer(uint8_t *tx, uint8_t *rx, uint16_t len)
{
    int idx = mock_spi_call_count;
    if (idx >= MOCK_SPI_MAX_TRANSFERS) return -1;

    /* Record TX bytes */
    uint16_t rec = (len > MOCK_SPI_MAX_BYTES) ? MOCK_SPI_MAX_BYTES : len;
    memcpy(mock_spi_log[idx].tx, tx, rec);
    mock_spi_log[idx].len = len;

    /* Return programmed RX bytes (zero-filled if not programmed) */
    memset(rx, 0, len);
    if (s_rx_len[idx] > 0) {
        uint16_t copy = (s_rx_len[idx] < len) ? s_rx_len[idx] : len;
        memcpy(rx, s_rx_buf[idx], copy);
    }

    mock_spi_call_count++;
    return 0;
}

hal_spi_t mock_hal_spi = { .transfer = mock_transfer };
