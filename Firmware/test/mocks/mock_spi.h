#ifndef MOCK_SPI_H
#define MOCK_SPI_H

#include <stdint.h>
#include "hal_spi.h"

/* Maximum number of SPI transfers recorded per test */
#define MOCK_SPI_MAX_TRANSFERS 32
#define MOCK_SPI_MAX_BYTES     8

typedef struct {
    uint8_t  tx[MOCK_SPI_MAX_BYTES];
    uint16_t len;
} mock_spi_record_t;

/* Log of actual transfers that occurred */
extern mock_spi_record_t mock_spi_log[MOCK_SPI_MAX_TRANSFERS];
extern int               mock_spi_call_count;

/* Reset log and all programmed responses — call at the start of each test */
void mock_spi_reset(void);

/* Pre-program the RX bytes returned for transfer number `idx` (0-based).
 * Unset transfers return all-zero bytes. */
void mock_spi_program_response(int idx, const uint8_t *rx, uint16_t len);

/* HAL instance wired to the mock — pass this to tmc5160_init() in tests */
extern hal_spi_t mock_hal_spi;

#endif /* MOCK_SPI_H */
