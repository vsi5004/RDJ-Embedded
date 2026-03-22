#ifndef CFG_FLASH_H
#define CFG_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_flash.h"

/* -------------------------------------------------------------------------
 * Flash layout — last 4KB of 128KB flash (STM32G431CB)
 * ------------------------------------------------------------------------- */
#define FLASH_SLOT_A_ADDR  0x0801F000u  /* 2KB config slot A */
#define FLASH_SLOT_B_ADDR  0x0801F800u  /* 2KB config slot B */
#define FLASH_SLOT_SIZE    0x0800u      /* 2KB per slot */

/* Magic word written to every valid config entry */
#define NODE_CONFIG_MAGIC  0xD4A50001u

/* -------------------------------------------------------------------------
 * Persistent per-node configuration struct
 *
 * Stored in both flash slots for power-loss-safe double-buffer updates.
 * Size is padded to 32 bytes (multiple of 8) for STM32G4 double-word writes.
 *
 * Field meanings:
 *   vmax       — TMC5160 default ramp velocity (microsteps/s)
 *   amax/dmax  — TMC5160 default acceleration/deceleration
 *   sgt        — StallGuard threshold (0 = most sensitive, 63 = least)
 *   pot_offset — A axis only: pot ADC counts when XACTUAL = 0 (set after homing)
 * ------------------------------------------------------------------------- */
typedef struct {
    uint32_t magic;       /* NODE_CONFIG_MAGIC when valid                */
    uint32_t seq;         /* Monotonic write counter; higher = newer     */
    uint32_t vmax;        /* TMC5160 VMAX default                        */
    uint16_t amax;        /* TMC5160 AMAX default                        */
    uint16_t dmax;        /* TMC5160 DMAX default                        */
    uint8_t  sgt;         /* StallGuard threshold                        */
    uint8_t  _pad[3];     /* alignment padding                           */
    int16_t  pot_offset;  /* A axis pot ADC offset at home position      */
    uint8_t  _pad2[2];    /* alignment padding                           */
    uint32_t checksum;    /* byte sum of all preceding fields            */
    uint32_t _pad3;       /* pad struct to 32 bytes for flash alignment  */
} node_config_t;          /* sizeof == 32 */

/* -------------------------------------------------------------------------
 * Hard-coded defaults — used on first boot and after double corruption
 * Conservative values: moderate velocity, low StallGuard sensitivity
 * ------------------------------------------------------------------------- */
extern const node_config_t cfg_flash_defaults;

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/* Bind flash HAL.  Must be called once before load/save. */
void cfg_flash_init(hal_flash_t *flash);

/* Load config from flash.
 * - Checks both slots; prefers the one with the higher sequence number.
 * - If neither slot is valid, copies cfg_flash_defaults into *out.
 * Returns true if a valid config was found in flash, false if defaults used. */
bool cfg_flash_load(node_config_t *out);

/* Persist config to the inactive flash slot, then erase the old slot.
 * Sets magic, increments seq, and computes checksum before writing. */
void cfg_flash_save(const node_config_t *cfg);

#endif /* CFG_FLASH_H */
