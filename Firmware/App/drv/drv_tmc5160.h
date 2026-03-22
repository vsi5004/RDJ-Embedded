#ifndef DRV_TMC5160_H
#define DRV_TMC5160_H

#include <stdint.h>
#include "hal_spi.h"

/* -------------------------------------------------------------------------
 * TMC5160 register addresses (SPI motion controller mode)
 * Ref: TMC5160 datasheet Rev 1.15
 * ------------------------------------------------------------------------- */
#define TMC5160_REG_GCONF       0x00
#define TMC5160_REG_GSTAT       0x01
#define TMC5160_REG_IHOLD_IRUN  0x10
#define TMC5160_REG_RAMPMODE    0x20
#define TMC5160_REG_XACTUAL     0x21
#define TMC5160_REG_VSTART      0x23
#define TMC5160_REG_AMAX        0x26
#define TMC5160_REG_VMAX        0x27
#define TMC5160_REG_DMAX        0x28
#define TMC5160_REG_VSTOP       0x2B
#define TMC5160_REG_XTARGET     0x2D
#define TMC5160_REG_SWMODE      0x34
#define TMC5160_REG_RAMPSTAT    0x35
#define TMC5160_REG_CHOPCONF    0x6C
#define TMC5160_REG_COOLCONF    0x6D
#define TMC5160_REG_DRV_STATUS  0x6F

/* RAMPMODE values */
#define TMC5160_RAMPMODE_POSITION  0
#define TMC5160_RAMPMODE_VEL_POS   1
#define TMC5160_RAMPMODE_VEL_NEG   2

/* SWMODE bit masks */
#define TMC5160_SWMODE_STOP_L_ENABLE  (1u << 0)
#define TMC5160_SWMODE_SG_STOP        (1u << 10)

/* RAMPSTAT bit masks */
#define TMC5160_RAMPSTAT_STATUS_STOP_L   (1u << 0)
#define TMC5160_RAMPSTAT_EVENT_STOP_L    (1u << 4)
#define TMC5160_RAMPSTAT_EVENT_STOP_SG   (1u << 6)
#define TMC5160_RAMPSTAT_EVENT_POS_REACH (1u << 7)
#define TMC5160_RAMPSTAT_VEL_REACHED     (1u << 8)
#define TMC5160_RAMPSTAT_POS_REACHED     (1u << 9)
#define TMC5160_RAMPSTAT_VZERO           (1u << 10)

/* DRV_STATUS bit masks */
#define TMC5160_DRV_STATUS_SG_RESULT_MASK  0x3FFu   /* bits 9:0 */
#define TMC5160_DRV_STATUS_STALL_GUARD     (1u << 24)

/* -------------------------------------------------------------------------
 * Driver configuration — passed to tmc5160_init().
 * Values sourced from flash config at runtime.
 * ------------------------------------------------------------------------- */
typedef struct {
    uint8_t  toff;        /* Chopper off time (1–15) */
    uint8_t  irun;        /* Run current (0–31, maps to % of CS) */
    uint8_t  ihold;       /* Hold current (0–31) */
    uint8_t  iholddelay;  /* Hold delay (0–15 × 2^18 clocks) */
    uint32_t vmax;        /* Max velocity (internal units) */
    uint16_t amax;        /* Max acceleration */
    uint16_t dmax;        /* Max deceleration */
    int8_t   sgt;         /* StallGuard2 threshold (-64 to +63) */
} tmc5160_config_t;

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/* Bind SPI instance and write initialisation registers.
 * Leaves motor in position mode (RAMPMODE=0), XACTUAL=0. */
void tmc5160_init(hal_spi_t *spi, const tmc5160_config_t *cfg);

/* Raw register access — 40-bit SPI transaction. */
void     tmc5160_write(uint8_t reg, uint32_t value);
uint32_t tmc5160_read (uint8_t reg);

/* Motion control */
void tmc5160_set_target  (int32_t  position);   /* → XTARGET */
void tmc5160_set_velocity(uint32_t vmax);        /* → VMAX    */
void tmc5160_set_accel   (uint16_t amax, uint16_t dmax); /* → AMAX, DMAX */
void tmc5160_set_rampmode(uint8_t  mode);        /* → RAMPMODE */

/* Status readback */
int32_t  tmc5160_get_position  (void);  /* ← XACTUAL */
uint32_t tmc5160_get_rampstat  (void);  /* ← RAMPSTAT */
uint16_t tmc5160_get_stallguard(void);  /* ← DRV_STATUS bits 9:0 */

/* Motor enable/disable via CHOPCONF.TOFF */
void tmc5160_disable(void);
void tmc5160_enable (void);

#endif /* DRV_TMC5160_H */
