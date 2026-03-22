/* Phase 3 — Homing state machine tests
 *
 * Tests cover:
 *   - IDLE: tick before start() is a no-op
 *   - start(): sets HOMING status bit, clears HOMED, writes TMC5160 velocity
 *     mode immediately (called from main-loop context, SPI is safe)
 *   - start() while active: ignored (no re-entrancy)
 *   - SEEKING: each tick polls RAMPSTAT; stays active without EVENT_STOP_L
 *   - Completion: EVENT_STOP_L → zero XACTUAL, restore position mode,
 *     clear HOMING, set HOMED, return to IDLE
 *   - Post-completion: tick is a no-op again
 *
 * setUp() initialises TMC5160, canopen, and homing modules fresh for each
 * test so there are no inter-test dependencies.
 *
 * Naming convention: test_homing_<scenario>
 */

#include "unity.h"
#include "app_homing.h"
#include "app_canopen.h"
#include "drv_tmc5160.h"
#include "mock_spi.h"

/* -------------------------------------------------------------------------
 * Test fixture
 * ------------------------------------------------------------------------- */

#define SEEK_VELOCITY  2000u

static const tmc5160_config_t CFG = {
    .toff = 5, .irun = 20, .ihold = 8, .iholddelay = 6,
    .vmax = 100000, .amax = 500, .dmax = 500, .sgt = 10,
};

void setUp(void)
{
    mock_spi_reset();
    tmc5160_init(&mock_hal_spi, &CFG);
    mock_spi_reset();   /* clear init traffic */
    canopen_init();
    app_homing_init(SEEK_VELOCITY);
}

void tearDown(void) {}

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static uint8_t  logged_reg(int i)      { return mock_spi_log[i].tx[0] & 0x7Fu; }
static int      logged_is_write(int i) { return (mock_spi_log[i].tx[0] & 0x80u) != 0; }

static uint32_t logged_data(int i)
{
    uint8_t *b = mock_spi_log[i].tx;
    return ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16)
         | ((uint32_t)b[3] <<  8) | (uint32_t)b[4];
}

static int find_write(uint8_t reg)
{
    for (int i = 0; i < mock_spi_call_count; i++) {
        if (logged_is_write(i) && logged_reg(i) == reg)
            return i;
    }
    return -1;
}

/* Program the Nth transfer to return val (for tmc5160_read() responses) */
static void program_response(int nth, uint32_t val)
{
    uint8_t resp[5] = {
        0x00,
        (uint8_t)(val >> 24), (uint8_t)(val >> 16),
        (uint8_t)(val >> 8),  (uint8_t)(val),
    };
    mock_spi_program_response(nth, resp, 5);
}

/* Program the RAMPSTAT response for the next tick() call.
 * tmc5160_get_rampstat() issues 2 transfers; the result is on the 2nd one.
 * Using mock_spi_call_count as the base means this is correct regardless of
 * how many transfers have already occurred — no log reset needed. */
static void program_rampstat(uint32_t val)
{
    program_response(mock_spi_call_count + 1, val);
}

/* Scan backwards for the most recent write to reg — useful when a register
 * is written more than once (e.g. RAMPMODE written by both start() and tick()) */
static int find_last_write(uint8_t reg)
{
    for (int i = mock_spi_call_count - 1; i >= 0; i--) {
        if (logged_is_write(i) && logged_reg(i) == reg)
            return i;
    }
    return -1;
}

/* =========================================================================
 * Group 1: IDLE — tick before start() is a no-op
 * ========================================================================= */

void test_homing_idle_tick_does_nothing(void)
{
    app_homing_tick();

    TEST_ASSERT_EQUAL_INT(0, mock_spi_call_count);
}

void test_homing_is_not_active_before_start(void)
{
    TEST_ASSERT_FALSE(app_homing_is_active());
}

/* =========================================================================
 * Group 2: start() — sets up motion immediately
 * ========================================================================= */

void test_homing_start_sets_homing_status_bit(void)
{
    app_homing_start();

    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_HOMING);
}

void test_homing_start_clears_previously_set_homed_bit(void)
{
    canopen_set_status_bit(CANOPEN_STATUS_HOMED);
    app_homing_start();

    TEST_ASSERT_FALSE(canopen_get_status_word() & CANOPEN_STATUS_HOMED);
}

void test_homing_start_writes_velocity_neg_mode(void)
{
    app_homing_start();

    int idx = find_write(TMC5160_REG_RAMPMODE);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "RAMPMODE not written on start");
    TEST_ASSERT_EQUAL_UINT32(TMC5160_RAMPMODE_VEL_NEG, logged_data(idx));
}

void test_homing_start_writes_seek_velocity(void)
{
    app_homing_start();

    int idx = find_write(TMC5160_REG_VMAX);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "VMAX not written on start");
    TEST_ASSERT_EQUAL_UINT32(SEEK_VELOCITY, logged_data(idx));
}

void test_homing_start_makes_is_active_true(void)
{
    app_homing_start();

    TEST_ASSERT_TRUE(app_homing_is_active());
}

void test_homing_start_while_active_is_ignored(void)
{
    app_homing_start();
    mock_spi_reset();

    app_homing_start();  /* second call while active */

    /* No additional SPI writes — start() was a no-op */
    TEST_ASSERT_EQUAL_INT(0, mock_spi_call_count);
}

/* =========================================================================
 * Group 3: SEEKING — tick polls RAMPSTAT, waits for endstop event
 * ========================================================================= */

void test_homing_seeking_tick_reads_rampstat(void)
{
    app_homing_start();
    mock_spi_reset();

    app_homing_tick();

    /* Should have performed at least one read of RAMPSTAT (2 SPI transfers) */
    TEST_ASSERT_GREATER_OR_EQUAL(2, mock_spi_call_count);
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_RAMPSTAT, mock_spi_log[0].tx[0] & 0x7Fu);
}

void test_homing_seeking_stays_active_without_event_stop_l(void)
{
    app_homing_start();
    program_rampstat(0);  /* no EVENT_STOP_L */
    app_homing_tick();

    TEST_ASSERT_TRUE(app_homing_is_active());
}

void test_homing_seeking_does_not_set_homed_without_event(void)
{
    app_homing_start();
    program_rampstat(0);
    app_homing_tick();

    TEST_ASSERT_FALSE(canopen_get_status_word() & CANOPEN_STATUS_HOMED);
}

/* =========================================================================
 * Group 4: Completion — EVENT_STOP_L triggers zeroing and cleanup
 * ========================================================================= */

void test_homing_completion_zeroes_xactual(void)
{
    app_homing_start();
    program_rampstat(TMC5160_RAMPSTAT_EVENT_STOP_L);
    app_homing_tick();

    int idx = find_write(TMC5160_REG_XACTUAL);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "XACTUAL not written on completion");
    TEST_ASSERT_EQUAL_UINT32(0, logged_data(idx));
}

void test_homing_completion_restores_position_mode(void)
{
    app_homing_start();
    program_rampstat(TMC5160_RAMPSTAT_EVENT_STOP_L);
    app_homing_tick();

    /* start() writes VEL_NEG; tick() completion writes POSITION — check the last one */
    int idx = find_last_write(TMC5160_REG_RAMPMODE);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "RAMPMODE not restored on completion");
    TEST_ASSERT_EQUAL_UINT32(TMC5160_RAMPMODE_POSITION, logged_data(idx));
}

void test_homing_completion_sets_homed_bit(void)
{
    app_homing_start();
    program_rampstat(TMC5160_RAMPSTAT_EVENT_STOP_L);
    app_homing_tick();

    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_HOMED);
}

void test_homing_completion_clears_homing_bit(void)
{
    app_homing_start();
    program_rampstat(TMC5160_RAMPSTAT_EVENT_STOP_L);
    app_homing_tick();

    TEST_ASSERT_FALSE(canopen_get_status_word() & CANOPEN_STATUS_HOMING);
}

void test_homing_completion_makes_is_active_false(void)
{
    app_homing_start();
    program_rampstat(TMC5160_RAMPSTAT_EVENT_STOP_L);
    app_homing_tick();

    TEST_ASSERT_FALSE(app_homing_is_active());
}

void test_homing_after_completion_tick_does_nothing(void)
{
    app_homing_start();
    program_rampstat(TMC5160_RAMPSTAT_EVENT_STOP_L);
    app_homing_tick();   /* completes homing */

    mock_spi_reset();
    app_homing_tick();   /* should be idle — no SPI */

    TEST_ASSERT_EQUAL_INT(0, mock_spi_call_count);
}

/* =========================================================================
 * Group 5: Re-homing — can start again after completion
 * ========================================================================= */

void test_homing_can_restart_after_completion(void)
{
    /* First homing */
    app_homing_start();
    program_rampstat(TMC5160_RAMPSTAT_EVENT_STOP_L);
    app_homing_tick();

    /* Second homing — should succeed */
    mock_spi_reset();
    app_homing_start();

    TEST_ASSERT_TRUE(app_homing_is_active());
    TEST_ASSERT_NOT_EQUAL(-1, find_write(TMC5160_REG_RAMPMODE));
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* IDLE */
    RUN_TEST(test_homing_idle_tick_does_nothing);
    RUN_TEST(test_homing_is_not_active_before_start);

    /* start() */
    RUN_TEST(test_homing_start_sets_homing_status_bit);
    RUN_TEST(test_homing_start_clears_previously_set_homed_bit);
    RUN_TEST(test_homing_start_writes_velocity_neg_mode);
    RUN_TEST(test_homing_start_writes_seek_velocity);
    RUN_TEST(test_homing_start_makes_is_active_true);
    RUN_TEST(test_homing_start_while_active_is_ignored);

    /* SEEKING */
    RUN_TEST(test_homing_seeking_tick_reads_rampstat);
    RUN_TEST(test_homing_seeking_stays_active_without_event_stop_l);
    RUN_TEST(test_homing_seeking_does_not_set_homed_without_event);

    /* Completion */
    RUN_TEST(test_homing_completion_zeroes_xactual);
    RUN_TEST(test_homing_completion_restores_position_mode);
    RUN_TEST(test_homing_completion_sets_homed_bit);
    RUN_TEST(test_homing_completion_clears_homing_bit);
    RUN_TEST(test_homing_completion_makes_is_active_false);
    RUN_TEST(test_homing_after_completion_tick_does_nothing);

    /* Re-homing */
    RUN_TEST(test_homing_can_restart_after_completion);

    return UNITY_END();
}
