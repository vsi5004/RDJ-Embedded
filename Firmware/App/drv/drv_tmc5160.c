#include "drv_tmc5160.h"
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Module state
 * ------------------------------------------------------------------------- */
static hal_spi_t *s_spi;
static uint32_t   s_chopconf;   /* Saved full CHOPCONF value for enable/disable */

/* -------------------------------------------------------------------------
 * Helpers to build register values from config fields
 * ------------------------------------------------------------------------- */

/* CHOPCONF: spreadCycle, TOFF, HSTRT=4, HEND=1, TBL=2, MRES=0 (256µstep),
 * INTPOL=1 (interpolation to 256).
 * bits  3:0  = TOFF
 * bits  6:4  = HSTRT (fixed 4)
 * bits 10:7  = HEND  (actual -1, stored as actual+3 = stored as 4 = 0b0100)
 * bits 15:14 = TBL   (fixed 2 → 36 clock blanking)
 * bit  28    = INTPOL (interpolation enable)
 */
static uint32_t build_chopconf(uint8_t toff)
{
    uint32_t v = 0;
    v |= (uint32_t)(toff & 0x0F);          /* TOFF      bits  3:0 */
    v |= (uint32_t)(4u)  << 4;             /* HSTRT=4   bits  6:4 */
    v |= (uint32_t)(4u)  << 7;             /* HEND=1→4  bits 10:7 */
    v |= (uint32_t)(2u)  << 14;            /* TBL=2     bits 15:14 */
    v |= (uint32_t)(1u)  << 28;            /* INTPOL    bit  28    */
    return v;
}

/* IHOLD_IRUN:
 * bits  4:0  = IHOLD
 * bits 12:8  = IRUN
 * bits 19:16 = IHOLDDELAY
 */
static uint32_t build_ihold_irun(uint8_t ihold, uint8_t irun, uint8_t iholddelay)
{
    return ((uint32_t)(iholddelay & 0x0F) << 16)
         | ((uint32_t)(irun       & 0x1F) <<  8)
         | ((uint32_t)(ihold      & 0x1F));
}

/* COOLCONF: SGT field in bits 30:24 (7-bit signed, sign-extend from int8_t).
 * Only SGT is written here; CoolStep is left disabled (semin=0).
 */
static uint32_t build_coolconf(int8_t sgt)
{
    return ((uint32_t)(sgt & 0x7F)) << 24;
}

/* -------------------------------------------------------------------------
 * Raw SPI access
 *
 * TMC5160 SPI protocol (40-bit = 5 bytes):
 *   Byte 0:   address — bit 7 = 1 for write, 0 for read
 *   Bytes 1–4: 32-bit data, MSB first
 *
 * On every transaction the chip simultaneously clocks out the SPI status byte
 * followed by the 32-bit data from the PREVIOUS address it received.
 * Therefore a reliable single-register read requires two transactions:
 *   1st: send read address → response is stale (ignore)
 *   2nd: send anything    → response contains the requested register value
 * ------------------------------------------------------------------------- */
void tmc5160_write(uint8_t reg, uint32_t value)
{
    uint8_t tx[5] = {
        (uint8_t)(reg | 0x80),
        (uint8_t)(value >> 24),
        (uint8_t)(value >> 16),
        (uint8_t)(value >>  8),
        (uint8_t)(value),
    };
    uint8_t rx[5];
    s_spi->transfer(tx, rx, 5);
}

uint32_t tmc5160_read(uint8_t reg)
{
    uint8_t tx[5] = { (uint8_t)(reg & 0x7F), 0, 0, 0, 0 };
    uint8_t rx[5] = { 0 };

    /* First transaction: send read address, discard response */
    s_spi->transfer(tx, rx, 5);

    /* Second transaction: clock out the actual register value */
    tx[0] = (uint8_t)(reg & 0x7F);
    s_spi->transfer(tx, rx, 5);

    return ((uint32_t)rx[1] << 24)
         | ((uint32_t)rx[2] << 16)
         | ((uint32_t)rx[3] <<  8)
         | ((uint32_t)rx[4]);
}

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */
void tmc5160_init(hal_spi_t *spi, const tmc5160_config_t *cfg)
{
    s_spi = spi;

    /* 1. GCONF — SPI mode, spreadCycle (en_pwm_mode=0) */
    tmc5160_write(TMC5160_REG_GCONF, 0x00000000);

    /* 2. CHOPCONF — chopper configuration */
    s_chopconf = build_chopconf(cfg->toff);
    tmc5160_write(TMC5160_REG_CHOPCONF, s_chopconf);

    /* 3. IHOLD_IRUN — current settings */
    tmc5160_write(TMC5160_REG_IHOLD_IRUN,
                  build_ihold_irun(cfg->ihold, cfg->irun, cfg->iholddelay));

    /* 4. SWMODE — left endstop enable + StallGuard stop */
    tmc5160_write(TMC5160_REG_SWMODE,
                  TMC5160_SWMODE_STOP_L_ENABLE | TMC5160_SWMODE_SG_STOP);

    /* 5. COOLCONF — StallGuard2 threshold (SGT) */
    tmc5160_write(TMC5160_REG_COOLCONF, build_coolconf(cfg->sgt));

    /* 6. Ramp parameters */
    tmc5160_write(TMC5160_REG_VSTART, 1);              /* avoid VSTOP≤VSTART issue */
    tmc5160_write(TMC5160_REG_VSTOP,  10);             /* must be > 0 in position mode */
    tmc5160_write(TMC5160_REG_VMAX,   cfg->vmax);
    tmc5160_write(TMC5160_REG_AMAX,   cfg->amax);
    tmc5160_write(TMC5160_REG_DMAX,   cfg->dmax);

    /* 7. Position mode */
    tmc5160_write(TMC5160_REG_RAMPMODE, TMC5160_RAMPMODE_POSITION);

    /* 8. Zero position counter — unhomed */
    tmc5160_write(TMC5160_REG_XACTUAL, 0);
}

/* -------------------------------------------------------------------------
 * Motion control
 * ------------------------------------------------------------------------- */
void tmc5160_set_target(int32_t position)
{
    tmc5160_write(TMC5160_REG_XTARGET, (uint32_t)position);
}

void tmc5160_set_velocity(uint32_t vmax)
{
    tmc5160_write(TMC5160_REG_VMAX, vmax);
}

void tmc5160_set_accel(uint16_t amax, uint16_t dmax)
{
    tmc5160_write(TMC5160_REG_AMAX, amax);
    tmc5160_write(TMC5160_REG_DMAX, dmax);
}

void tmc5160_set_rampmode(uint8_t mode)
{
    tmc5160_write(TMC5160_REG_RAMPMODE, mode);
}

/* -------------------------------------------------------------------------
 * Status readback
 * ------------------------------------------------------------------------- */
int32_t tmc5160_get_position(void)
{
    return (int32_t)tmc5160_read(TMC5160_REG_XACTUAL);
}

uint32_t tmc5160_get_rampstat(void)
{
    return tmc5160_read(TMC5160_REG_RAMPSTAT);
}

uint16_t tmc5160_get_stallguard(void)
{
    uint32_t drv = tmc5160_read(TMC5160_REG_DRV_STATUS);
    return (uint16_t)(drv & TMC5160_DRV_STATUS_SG_RESULT_MASK);
}

/* -------------------------------------------------------------------------
 * Enable / disable motor via CHOPCONF.TOFF
 * TOFF=0 disables the driver output; restoring the saved value re-enables it.
 * The motor is NOT de-energised — holding torque is maintained through the
 * driver's internal state.  For a full de-energise, use IHOLD=0 instead.
 * ------------------------------------------------------------------------- */
void tmc5160_disable(void)
{
    /* Write CHOPCONF with TOFF bits (3:0) cleared */
    tmc5160_write(TMC5160_REG_CHOPCONF, s_chopconf & ~0x0Fu);
}

void tmc5160_enable(void)
{
    tmc5160_write(TMC5160_REG_CHOPCONF, s_chopconf);
}
