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

/* Address and sequence number of the currently active (most recent) slot.
 * When neither slot is valid (first boot), returns addr=FLASH_SLOT_B_ADDR
 * and seq=0 so that the first write targets slot A. */
typedef struct { uint32_t addr; uint32_t seq; } slot_loc_t;

static slot_loc_t find_active_slot(const node_config_t *slot_a, bool a_valid,
                                    const node_config_t *slot_b, bool b_valid)
{
    if (a_valid && b_valid) {
        return (slot_a->seq >= slot_b->seq)
            ? (slot_loc_t){ FLASH_SLOT_A_ADDR, slot_a->seq }
            : (slot_loc_t){ FLASH_SLOT_B_ADDR, slot_b->seq };
    }
    if (a_valid) { return (slot_loc_t){ FLASH_SLOT_A_ADDR, slot_a->seq }; }
    if (b_valid) { return (slot_loc_t){ FLASH_SLOT_B_ADDR, slot_b->seq }; }
    return (slot_loc_t){ FLASH_SLOT_B_ADDR, 0 };  /* first boot → write to A */
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

    if (!a_valid && !b_valid) {
        *out = cfg_flash_defaults;
        return false;
    }

    slot_loc_t active = find_active_slot(&slot_a, a_valid, &slot_b, b_valid);
    *out = (active.addr == FLASH_SLOT_A_ADDR) ? slot_a : slot_b;
    return true;
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

    slot_loc_t active = find_active_slot(&slot_a, a_valid, &slot_b, b_valid);

    uint32_t target = (active.addr == FLASH_SLOT_A_ADDR)
                    ? FLASH_SLOT_B_ADDR
                    : FLASH_SLOT_A_ADDR;

    /* Build the new config entry */
    node_config_t to_write = *cfg;
    to_write.magic    = NODE_CONFIG_MAGIC;
    to_write.seq      = active.seq + 1u;
    to_write.checksum = compute_checksum(&to_write);

    s_flash->write(target, (const uint8_t *)&to_write, sizeof(to_write));
    s_flash->erase(active.addr);
}
