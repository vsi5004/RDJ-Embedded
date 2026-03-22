/* Double-buffer flash config driver tests
 *
 * Tests cover:
 *   - Load from blank flash: uses hardcoded defaults, returns false
 *   - Round-trip: save then load recovers all fields exactly
 *   - Double-buffer alternation: first save → slot A; second → slot B; third → A
 *   - Erase of previous slot: after each save the old slot is wiped to 0xFF
 *   - Corruption recovery: one slot corrupted → loads from the other
 *   - Both slots corrupted → falls back to defaults
 *   - Sequence number: when both slots are valid, load picks the higher seq
 *
 * setUp() resets the mock flash (fills with 0xFF = erased state) and
 * re-inits cfg_flash so every test starts from first-boot conditions.
 *
 * Helper make_valid_config() constructs a node_config_t with correct
 * magic and checksum so tests can pre-populate mock_flash_mem directly
 * to set up multi-slot scenarios without going through cfg_flash_save().
 *
 * Naming convention: test_flash_<scenario>
 */

#include "unity.h"
#include "cfg_flash.h"
#include "mock_flash.h"
#include <string.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Test fixture
 * ------------------------------------------------------------------------- */

void setUp(void)
{
    mock_flash_reset();
    cfg_flash_init(&mock_hal_flash);
}

void tearDown(void) {}

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/* Byte-sum checksum — mirrors cfg_flash.c's compute_checksum() */
static uint32_t test_checksum(const node_config_t *cfg)
{
    const uint8_t *p = (const uint8_t *)cfg;
    uint32_t sum = 0;
    for (size_t i = 0; i < offsetof(node_config_t, checksum); i++) {
        sum += p[i];
    }
    return sum;
}

/* Build a fully valid config (magic + seq + checksum all correct) */
static node_config_t make_valid_config(uint32_t seq, uint32_t vmax)
{
    node_config_t cfg = {0};
    cfg.magic      = NODE_CONFIG_MAGIC;
    cfg.seq        = seq;
    cfg.vmax       = vmax;
    cfg.amax       = 500u;
    cfg.dmax       = 500u;
    cfg.sgt        = 10u;
    cfg.pot_offset = 0;
    cfg.checksum   = test_checksum(&cfg);
    return cfg;
}

/* Write a config struct directly into the simulated flash at slot_addr,
 * bypassing cfg_flash_save() — used to set up multi-slot test scenarios. */
static void poke_slot(uint32_t slot_addr, const node_config_t *cfg)
{
    uint32_t offset = slot_addr - MOCK_FLASH_BASE;
    memcpy(mock_flash_mem + offset, cfg, sizeof(*cfg));
}

/* Read a config struct back from the simulated flash for assertion */
static node_config_t peek_slot(uint32_t slot_addr)
{
    node_config_t cfg;
    uint32_t offset = slot_addr - MOCK_FLASH_BASE;
    memcpy(&cfg, mock_flash_mem + offset, sizeof(cfg));
    return cfg;
}

/* True if the 2KB slot at slot_addr is fully erased (all 0xFF) */
static bool slot_is_erased(uint32_t slot_addr)
{
    uint32_t offset = slot_addr - MOCK_FLASH_BASE;
    for (uint32_t i = 0; i < FLASH_SLOT_SIZE; i++) {
        if (mock_flash_mem[offset + i] != 0xFFu) return false;
    }
    return true;
}

/* =========================================================================
 * Group 1: Load from blank flash → defaults
 * ========================================================================= */

void test_flash_load_from_blank_returns_false(void)
{
    node_config_t out;
    TEST_ASSERT_FALSE(cfg_flash_load(&out));
}

void test_flash_load_from_blank_returns_default_vmax(void)
{
    node_config_t out;
    cfg_flash_load(&out);
    TEST_ASSERT_EQUAL_UINT32(cfg_flash_defaults.vmax, out.vmax);
}

void test_flash_load_from_blank_returns_default_sgt(void)
{
    node_config_t out;
    cfg_flash_load(&out);
    TEST_ASSERT_EQUAL_UINT8(cfg_flash_defaults.sgt, out.sgt);
}

/* =========================================================================
 * Group 2: Round-trip — save then load recovers every field
 * ========================================================================= */

void test_flash_roundtrip_vmax(void)
{
    node_config_t cfg = cfg_flash_defaults;
    cfg.vmax = 150000u;
    cfg_flash_save(&cfg);

    node_config_t out;
    TEST_ASSERT_TRUE(cfg_flash_load(&out));
    TEST_ASSERT_EQUAL_UINT32(150000u, out.vmax);
}

void test_flash_roundtrip_amax(void)
{
    node_config_t cfg = cfg_flash_defaults;
    cfg.amax = 1200u;
    cfg_flash_save(&cfg);

    node_config_t out;
    cfg_flash_load(&out);
    TEST_ASSERT_EQUAL_UINT16(1200u, out.amax);
}

void test_flash_roundtrip_dmax(void)
{
    node_config_t cfg = cfg_flash_defaults;
    cfg.dmax = 800u;
    cfg_flash_save(&cfg);

    node_config_t out;
    cfg_flash_load(&out);
    TEST_ASSERT_EQUAL_UINT16(800u, out.dmax);
}

void test_flash_roundtrip_sgt(void)
{
    node_config_t cfg = cfg_flash_defaults;
    cfg.sgt = 25u;
    cfg_flash_save(&cfg);

    node_config_t out;
    cfg_flash_load(&out);
    TEST_ASSERT_EQUAL_UINT8(25u, out.sgt);
}

void test_flash_roundtrip_pot_offset(void)
{
    node_config_t cfg = cfg_flash_defaults;
    cfg.pot_offset = -42;
    cfg_flash_save(&cfg);

    node_config_t out;
    cfg_flash_load(&out);
    TEST_ASSERT_EQUAL_INT16(-42, out.pot_offset);
}

/* =========================================================================
 * Group 3: Double-buffer alternation
 * ========================================================================= */

void test_flash_first_save_writes_to_slot_a(void)
{
    cfg_flash_save(&cfg_flash_defaults);

    node_config_t in_a = peek_slot(FLASH_SLOT_A_ADDR);
    TEST_ASSERT_EQUAL_UINT32(NODE_CONFIG_MAGIC, in_a.magic);
}

void test_flash_second_save_writes_to_slot_b(void)
{
    cfg_flash_save(&cfg_flash_defaults);

    node_config_t cfg2 = cfg_flash_defaults;
    cfg2.vmax = 77777u;
    cfg_flash_save(&cfg2);

    node_config_t in_b = peek_slot(FLASH_SLOT_B_ADDR);
    TEST_ASSERT_EQUAL_UINT32(NODE_CONFIG_MAGIC, in_b.magic);
    TEST_ASSERT_EQUAL_UINT32(77777u, in_b.vmax);
}

void test_flash_third_save_writes_to_slot_a(void)
{
    cfg_flash_save(&cfg_flash_defaults);
    cfg_flash_save(&cfg_flash_defaults);

    node_config_t cfg3 = cfg_flash_defaults;
    cfg3.vmax = 33333u;
    cfg_flash_save(&cfg3);

    node_config_t in_a = peek_slot(FLASH_SLOT_A_ADDR);
    TEST_ASSERT_EQUAL_UINT32(NODE_CONFIG_MAGIC, in_a.magic);
    TEST_ASSERT_EQUAL_UINT32(33333u, in_a.vmax);
}

void test_flash_save_erases_previous_slot(void)
{
    cfg_flash_save(&cfg_flash_defaults);  /* → slot A; erases B (already blank) */
    cfg_flash_save(&cfg_flash_defaults);  /* → slot B; erases slot A */

    TEST_ASSERT_TRUE(slot_is_erased(FLASH_SLOT_A_ADDR));
}

void test_flash_seq_increments_on_each_save(void)
{
    cfg_flash_save(&cfg_flash_defaults);
    cfg_flash_save(&cfg_flash_defaults);
    cfg_flash_save(&cfg_flash_defaults);

    node_config_t in_a = peek_slot(FLASH_SLOT_A_ADDR);
    TEST_ASSERT_EQUAL_UINT32(3u, in_a.seq);
}

/* =========================================================================
 * Group 4: Corruption recovery
 * ========================================================================= */

void test_flash_load_with_slot_a_corrupted_uses_slot_b(void)
{
    /* Pre-populate both slots directly so both are "valid" */
    node_config_t cfg_a = make_valid_config(1, 11111u);
    node_config_t cfg_b = make_valid_config(2, 22222u);
    poke_slot(FLASH_SLOT_A_ADDR, &cfg_a);
    poke_slot(FLASH_SLOT_B_ADDR, &cfg_b);

    /* Corrupt slot A by overwriting its magic word */
    mock_flash_mem[FLASH_SLOT_A_ADDR - MOCK_FLASH_BASE] = 0xFFu;

    node_config_t out;
    TEST_ASSERT_TRUE(cfg_flash_load(&out));
    TEST_ASSERT_EQUAL_UINT32(22222u, out.vmax);
}

void test_flash_load_with_slot_b_corrupted_uses_slot_a(void)
{
    node_config_t cfg_a = make_valid_config(1, 11111u);
    node_config_t cfg_b = make_valid_config(2, 22222u);
    poke_slot(FLASH_SLOT_A_ADDR, &cfg_a);
    poke_slot(FLASH_SLOT_B_ADDR, &cfg_b);

    /* Corrupt slot B */
    mock_flash_mem[FLASH_SLOT_B_ADDR - MOCK_FLASH_BASE] = 0xFFu;

    node_config_t out;
    TEST_ASSERT_TRUE(cfg_flash_load(&out));
    TEST_ASSERT_EQUAL_UINT32(11111u, out.vmax);
}

void test_flash_load_with_both_corrupted_returns_defaults(void)
{
    node_config_t cfg_a = make_valid_config(1, 11111u);
    node_config_t cfg_b = make_valid_config(2, 22222u);
    poke_slot(FLASH_SLOT_A_ADDR, &cfg_a);
    poke_slot(FLASH_SLOT_B_ADDR, &cfg_b);

    mock_flash_mem[FLASH_SLOT_A_ADDR - MOCK_FLASH_BASE] = 0xFFu;
    mock_flash_mem[FLASH_SLOT_B_ADDR - MOCK_FLASH_BASE] = 0xFFu;

    node_config_t out;
    TEST_ASSERT_FALSE(cfg_flash_load(&out));
    TEST_ASSERT_EQUAL_UINT32(cfg_flash_defaults.vmax, out.vmax);
}

/* =========================================================================
 * Group 5: Sequence number arbitration (both slots valid)
 * ========================================================================= */

void test_flash_load_prefers_higher_seq(void)
{
    /* Slot A has seq=5, slot B has seq=7 → load should pick slot B */
    node_config_t cfg_a = make_valid_config(5, 55555u);
    node_config_t cfg_b = make_valid_config(7, 77777u);
    poke_slot(FLASH_SLOT_A_ADDR, &cfg_a);
    poke_slot(FLASH_SLOT_B_ADDR, &cfg_b);

    node_config_t out;
    cfg_flash_load(&out);
    TEST_ASSERT_EQUAL_UINT32(77777u, out.vmax);
}

void test_flash_save_after_both_valid_writes_to_lower_seq_slot(void)
{
    /* Pre-populate: A=seq5, B=seq7 → active=B → next write should go to A */
    node_config_t cfg_a = make_valid_config(5, 55555u);
    node_config_t cfg_b = make_valid_config(7, 77777u);
    poke_slot(FLASH_SLOT_A_ADDR, &cfg_a);
    poke_slot(FLASH_SLOT_B_ADDR, &cfg_b);

    node_config_t to_save = cfg_flash_defaults;
    to_save.vmax = 88888u;
    cfg_flash_save(&to_save);

    /* New write must have gone to slot A (overwriting seq=5) with seq=8 */
    node_config_t in_a = peek_slot(FLASH_SLOT_A_ADDR);
    TEST_ASSERT_EQUAL_UINT32(NODE_CONFIG_MAGIC, in_a.magic);
    TEST_ASSERT_EQUAL_UINT32(8u, in_a.seq);
    TEST_ASSERT_EQUAL_UINT32(88888u, in_a.vmax);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* blank flash → defaults */
    RUN_TEST(test_flash_load_from_blank_returns_false);
    RUN_TEST(test_flash_load_from_blank_returns_default_vmax);
    RUN_TEST(test_flash_load_from_blank_returns_default_sgt);

    /* round-trip */
    RUN_TEST(test_flash_roundtrip_vmax);
    RUN_TEST(test_flash_roundtrip_amax);
    RUN_TEST(test_flash_roundtrip_dmax);
    RUN_TEST(test_flash_roundtrip_sgt);
    RUN_TEST(test_flash_roundtrip_pot_offset);

    /* double-buffer alternation */
    RUN_TEST(test_flash_first_save_writes_to_slot_a);
    RUN_TEST(test_flash_second_save_writes_to_slot_b);
    RUN_TEST(test_flash_third_save_writes_to_slot_a);
    RUN_TEST(test_flash_save_erases_previous_slot);
    RUN_TEST(test_flash_seq_increments_on_each_save);

    /* corruption recovery */
    RUN_TEST(test_flash_load_with_slot_a_corrupted_uses_slot_b);
    RUN_TEST(test_flash_load_with_slot_b_corrupted_uses_slot_a);
    RUN_TEST(test_flash_load_with_both_corrupted_returns_defaults);

    /* sequence arbitration */
    RUN_TEST(test_flash_load_prefers_higher_seq);
    RUN_TEST(test_flash_save_after_both_valid_writes_to_lower_seq_slot);

    return UNITY_END();
}
