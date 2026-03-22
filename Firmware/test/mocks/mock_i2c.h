#ifndef MOCK_I2C_H
#define MOCK_I2C_H

#include <stdint.h>
#include "hal_i2c.h"

#define MOCK_I2C_MAX_TRANSFERS 32
#define MOCK_I2C_MAX_BYTES     32

typedef struct {
    uint8_t  addr;
    uint8_t  data[MOCK_I2C_MAX_BYTES];
    uint16_t len;
    int      is_read;
} mock_i2c_record_t;

extern mock_i2c_record_t mock_i2c_log[MOCK_I2C_MAX_TRANSFERS];
extern int               mock_i2c_call_count;

void mock_i2c_reset(void);
void mock_i2c_program_read_response(int idx, const uint8_t *data, uint16_t len);

extern hal_i2c_t mock_hal_i2c;

#endif /* MOCK_I2C_H */
