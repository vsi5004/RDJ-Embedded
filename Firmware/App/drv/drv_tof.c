#include "drv_tof.h"

/* -------------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------------- */

static hal_i2c_t *s_i2c;

/* -------------------------------------------------------------------------
 * Helpers — 16-bit register addressing (MSB-first)
 * ------------------------------------------------------------------------- */

static void reg_write8(uint16_t reg, uint8_t val)
{
    uint8_t buf[3] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFFu),
        val,
    };
    s_i2c->write(TOF_I2C_ADDR, buf, 3);
}

static void reg_read(uint16_t reg, uint8_t *rx, uint16_t len)
{
    uint8_t addr[2] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFFu),
    };
    s_i2c->write_read(TOF_I2C_ADDR, addr, 2, rx, len);
}

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

void drv_tof_init(hal_i2c_t *i2c)
{
    s_i2c = i2c;

    /* Minimal configuration for continuous ranging.
     * PAD_I2C_HV__EXTSUP_CONFIG: use internal 2.8 V supply on XSHUT.
     * GPIO__HV_MUX_CTRL: route new-data interrupt to GPIO1 pin. */
    reg_write8(0x002Du, 0x01u);
    reg_write8(0x002Eu, 0x00u);
}

/* -------------------------------------------------------------------------
 * Ranging control
 * ------------------------------------------------------------------------- */

void drv_tof_start_ranging(void)
{
    reg_write8(TOF_REG_START, 0x40u);
}

bool drv_tof_is_data_ready(void)
{
    uint8_t status = 0x01u;  /* default to not ready */
    reg_read(TOF_REG_STATUS, &status, 1);
    return (status & 0x01u) == 0;
}

uint16_t drv_tof_get_distance_mm(void)
{
    uint8_t buf[2] = {0, 0};
    reg_read(TOF_REG_RANGE_MM, buf, 2);
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

void drv_tof_clear_interrupt(void)
{
    reg_write8(TOF_REG_CLEAR_INT, 0x01u);
}
