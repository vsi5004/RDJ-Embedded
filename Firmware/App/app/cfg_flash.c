#include "cfg_flash.h"
#include <string.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Defaults
 * ------------------------------------------------------------------------- */

const node_config_t cfg_flash_defaults = {
    .magic      = 0,
    .seq        = 0,
    .vmax       = 100000u,
    .amax       = 500u,
    .dmax       = 500u,
    .sgt        = 10u,
    .pot_offset = 0,
    .checksum   = 0,
};

/* -------------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------------- */

static hal_flash_t *s_flash;

/* -------------------------------------------------------------------------
 * Checksum — byte sum of every byte before the checksum field
 * ------------------------------------------------------------------------- */

static uint32_t compute_checksum(const node_config_t *cfg)
{
    const uint8_t *p = (const uint8_t *)cfg;
    uint32_t sum = 0;
    for (size_t i = 0; i < offsetof(node_config_t, checksum); i++) {
        sum += p[i];
    }
    return sum;
}

/* -------------------------------------------------------------------------
 * Slot helpers
 * ------------------------------------------------------------------------- */

static void read_slot(uint32_t addr, node_config_t *out)
{
    s_flash->read(addr, (uint8_t *)out, sizeof(node_config_t));
}

static bool is_valid(const node_config_t *cfg)
{
    return cfg->magic == NODE_CONFIG_MAGIC &&
           cfg->checksum == compute_checksum(cfg);
}

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

void cfg_flash_init(hal_flash_t *flash)
{
    s_flash = flash;
}

/* -------------------------------------------------------------------------
 * Load
 *
 * Priority order:
 *   1. If both slots valid → use the one with the higher seq (more recent)
 *   2. If only one slot valid → use that one
 *   3. If neither valid → copy defaults, return false
 * ------------------------------------------------------------------------- */

bool cfg_flash_load(node_config_t *out)
{
    node_config_t slot_a, slot_b;
    read_slot(FLASH_SLOT_A_ADDR, &slot_a);
    read_slot(FLASH_SLOT_B_ADDR, &slot_b);

    bool a_valid = is_valid(&slot_a);
    bool b_valid = is_valid(&slot_b);

    if (a_valid && b_valid) {
        *out = (slot_a.seq >= slot_b.seq) ? slot_a : slot_b;
        return true;
    }
    if (a_valid) {
        *out = slot_a;
        return true;
    }
    if (b_valid) {
        *out = slot_b;
        return true;
    }

    *out = cfg_flash_defaults;
    return false;
}

/* -------------------------------------------------------------------------
 * Save
 *
 * Algorithm:
 *   1. Read both slots to find the currently active one (valid + highest seq)
 *   2. Target slot = the OTHER one
 *   3. Write new config (with incremented seq and computed checksum) to target
 *   4. Erase the previously active slot
 *
 * After save: exactly one slot is valid (the newly written one).
 * The erased slot will be used on the next save.
 * ------------------------------------------------------------------------- */

void cfg_flash_save(const node_config_t *cfg)
{
    node_config_t slot_a, slot_b;
    read_slot(FLASH_SLOT_A_ADDR, &slot_a);
    read_slot(FLASH_SLOT_B_ADDR, &slot_b);

    bool a_valid = is_valid(&slot_a);
    bool b_valid = is_valid(&slot_b);

    /* Determine the currently active slot address and its sequence number */
    uint32_t active_addr;
    uint32_t active_seq;

    if (a_valid && b_valid) {
        /* Both valid (power cut between write and erase). Active = higher seq. */
        if (slot_a.seq >= slot_b.seq) {
            active_addr = FLASH_SLOT_A_ADDR;
            active_seq  = slot_a.seq;
        } else {
            active_addr = FLASH_SLOT_B_ADDR;
            active_seq  = slot_b.seq;
        }
    } else if (a_valid) {
        active_addr = FLASH_SLOT_A_ADDR;
        active_seq  = slot_a.seq;
    } else if (b_valid) {
        active_addr = FLASH_SLOT_B_ADDR;
        active_seq  = slot_b.seq;
    } else {
        /* Neither valid (first boot) — write to A first, treating B as active */
        active_addr = FLASH_SLOT_B_ADDR;
        active_seq  = 0;
    }

    uint32_t target = (active_addr == FLASH_SLOT_A_ADDR)
                    ? FLASH_SLOT_B_ADDR
                    : FLASH_SLOT_A_ADDR;

    /* Build the new config entry */
    node_config_t to_write = *cfg;
    to_write.magic    = NODE_CONFIG_MAGIC;
    to_write.seq      = active_seq + 1u;
    to_write.checksum = compute_checksum(&to_write);

    s_flash->write(target, (const uint8_t *)&to_write, sizeof(to_write));
    s_flash->erase(active_addr);
}
