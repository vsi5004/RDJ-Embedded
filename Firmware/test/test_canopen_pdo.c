/* Phase 2 — CANopen PDO staging / SYNC application tests
 *
 * Tests cover:
 *   - RPDO staging: values buffered, NOT written to TMC5160 until SYNC
 *   - SYNC application: staged RPDO atomically applied on SYNC
 *   - TPDO latching: sensor snapshot captured on SYNC
 *   - Status word: MOVING and IN_POSITION derived from RAMPSTAT
 *   - Control word: ENABLE, HALT, CLEAR_FAULT actions
 *   - Status word API: set/clear bit helpers
 *
 * Each test calls setUp() which resets the mock SPI, re-inits both the
 * TMC5160 driver and the CANopen module, then clears the SPI log so
 * init traffic is invisible to test assertions.
 *
 * Naming convention: test_<group>_<scenario>
 */

#include "unity.h"
#include "app_canopen.h"
#include "drv_tmc5160.h"
#include "mock_spi.h"

/* -------------------------------------------------------------------------
 * Test fixture
 * ------------------------------------------------------------------------- */

static const tmc5160_config_t CFG = {
    .toff = 5, .irun = 20, .ihold = 8, .iholddelay = 6,
    .vmax = 100000, .amax = 500, .dmax = 500, .sgt = 10,
};

void setUp(void)
{
    mock_spi_reset();
    tmc5160_init(&mock_hal_spi, &CFG);
    mock_spi_reset();   /* clear init traffic — tests start from a clean log */
    canopen_init();
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

/* Scan log for first write to reg; returns index or -1 if not found */
static int find_write(uint8_t reg)
{
    for (int i = 0; i < mock_spi_call_count; i++) {
        if (logged_is_write(i) && logged_reg(i) == reg)
            return i;
    }
    return -1;
}

/* Program the Nth SPI transfer to return a 32-bit value (for read responses) */
static void program_response(int nth, uint32_t val)
{
    uint8_t resp[5] = {
        0x00,
        (uint8_t)(val >> 24), (uint8_t)(val >> 16),
        (uint8_t)(val >> 8),  (uint8_t)(val),
    };
    mock_spi_program_response(nth, resp, 5);
}

/* canopen_sync_process() reads RAMPSTAT then XACTUAL before applying RPDO.
 * Each tmc5160_read() uses 2 SPI transfers; the result comes back on transfer 1.
 *   Transfer 0, 1 → RAMPSTAT (data at index 1)
 *   Transfer 2, 3 → XACTUAL  (data at index 3)
 */
static void program_sync_reads(uint32_t rampstat_val, int32_t xactual_val)
{
    program_response(1, rampstat_val);
    program_response(3, (uint32_t)xactual_val);
}

/* =========================================================================
 * Group 1: RPDO Staging — values must not reach TMC5160 before SYNC
 * ========================================================================= */

void test_rpdo_staging_does_not_trigger_spi(void)
{
    canopen_stepper_rpdo_write(5000, 80000, 0, TMC5160_RAMPMODE_POSITION);

    TEST_ASSERT_EQUAL_INT(0, mock_spi_call_count);
}

void test_rpdo_staging_second_write_overwrites_first(void)
{
    /* Write twice — only the second values should appear on SYNC. */
    canopen_stepper_rpdo_write(1111, 10000, 0, TMC5160_RAMPMODE_POSITION);
    canopen_stepper_rpdo_write(9999, 20000, 0, TMC5160_RAMPMODE_VEL_POS);

    canopen_sync_process();

    int idx = find_write(TMC5160_REG_XTARGET);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "XTARGET not written on SYNC");
    TEST_ASSERT_EQUAL_INT32(9999, (int32_t)logged_data(idx));
}

/* =========================================================================
 * Group 2: SYNC application — staged RPDO written to TMC5160
 * ========================================================================= */

void test_sync_writes_xtarget_from_staged_rpdo(void)
{
    canopen_stepper_rpdo_write(12345, 80000, 0, TMC5160_RAMPMODE_POSITION);
    canopen_sync_process();

    int idx = find_write(TMC5160_REG_XTARGET);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "XTARGET not written");
    TEST_ASSERT_EQUAL_INT32(12345, (int32_t)logged_data(idx));
}

void test_sync_writes_negative_xtarget(void)
{
    canopen_stepper_rpdo_write(-4096, 80000, 0, TMC5160_RAMPMODE_POSITION);
    canopen_sync_process();

    int idx = find_write(TMC5160_REG_XTARGET);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "XTARGET not written");
    TEST_ASSERT_EQUAL_INT32(-4096, (int32_t)logged_data(idx));
}

void test_sync_writes_vmax_from_staged_rpdo(void)
{
    canopen_stepper_rpdo_write(0, 55555, 0, TMC5160_RAMPMODE_POSITION);
    canopen_sync_process();

    int idx = find_write(TMC5160_REG_VMAX);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "VMAX not written");
    TEST_ASSERT_EQUAL_UINT32(55555, logged_data(idx));
}

void test_sync_writes_rampmode_from_staged_rpdo(void)
{
    canopen_stepper_rpdo_write(0, 80000, 0, TMC5160_RAMPMODE_VEL_POS);
    canopen_sync_process();

    int idx = find_write(TMC5160_REG_RAMPMODE);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "RAMPMODE not written");
    TEST_ASSERT_EQUAL_UINT32(TMC5160_RAMPMODE_VEL_POS, logged_data(idx));
}

void test_sync_without_pending_rpdo_does_not_write_xtarget(void)
{
    /* No rpdo_write() called — SYNC should read status but not write XTARGET */
    canopen_sync_process();

    TEST_ASSERT_EQUAL_INT(-1, find_write(TMC5160_REG_XTARGET));
}

void test_sync_clears_pending_second_sync_does_not_reapply(void)
{
    canopen_stepper_rpdo_write(5000, 80000, 0, TMC5160_RAMPMODE_POSITION);
    canopen_sync_process();   /* first SYNC — applies the RPDO */

    mock_spi_reset();
    canopen_sync_process();   /* second SYNC — nothing pending */

    TEST_ASSERT_EQUAL_INT(-1, find_write(TMC5160_REG_XTARGET));
}

/* =========================================================================
 * Group 3: TPDO latching — sensor snapshot captured on SYNC
 * ========================================================================= */

void test_tpdo_latches_actual_position(void)
{
    program_sync_reads(0, 98765);
    canopen_sync_process();

    TEST_ASSERT_EQUAL_INT32(98765, canopen_get_tpdo_stepper()->actual_position);
}

void test_tpdo_latches_negative_position(void)
{
    program_sync_reads(0, -2048);
    canopen_sync_process();

    TEST_ASSERT_EQUAL_INT32(-2048, canopen_get_tpdo_stepper()->actual_position);
}

void test_tpdo_latches_ramp_status_byte(void)
{
    /* Set RAMPSTAT value; lower 8 bits should appear in tpdo->ramp_status */
    program_sync_reads(0x5A, 0);
    canopen_sync_process();

    TEST_ASSERT_EQUAL_UINT8(0x5A, canopen_get_tpdo_stepper()->ramp_status);
}

void test_tpdo_latches_tof_distance(void)
{
    canopen_update_tof(423);
    canopen_sync_process();

    TEST_ASSERT_EQUAL_UINT16(423, canopen_get_tpdo_stepper()->tof_distance_mm);
}

void test_tpdo_tof_distance_zero_by_default(void)
{
    canopen_sync_process();

    TEST_ASSERT_EQUAL_UINT16(0, canopen_get_tpdo_stepper()->tof_distance_mm);
}

/* =========================================================================
 * Group 4: Status word — MOVING and IN_POSITION from RAMPSTAT
 * ========================================================================= */

void test_status_moving_set_when_vzero_clear(void)
{
    /* RAMPSTAT with VZERO=0 → axis is moving */
    program_sync_reads(0, 0);   /* no VZERO bit */
    canopen_sync_process();

    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_MOVING);
}

void test_status_moving_clear_when_vzero_set(void)
{
    /* RAMPSTAT with VZERO=1 → velocity is zero → not moving */
    program_sync_reads(TMC5160_RAMPSTAT_VZERO, 0);
    canopen_sync_process();

    TEST_ASSERT_FALSE(canopen_get_status_word() & CANOPEN_STATUS_MOVING);
}

void test_status_in_position_set_when_pos_reached(void)
{
    program_sync_reads(TMC5160_RAMPSTAT_POS_REACHED, 0);
    canopen_sync_process();

    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_IN_POSITION);
}

void test_status_in_position_clear_when_pos_not_reached(void)
{
    program_sync_reads(0, 0);
    canopen_sync_process();

    TEST_ASSERT_FALSE(canopen_get_status_word() & CANOPEN_STATUS_IN_POSITION);
}

void test_tpdo_status_word_captures_pre_sync_status(void)
{
    /* Set a status bit manually, then verify it appears in the latched TPDO */
    canopen_set_status_bit(CANOPEN_STATUS_HOMED);
    canopen_sync_process();

    TEST_ASSERT_TRUE(canopen_get_tpdo_stepper()->status_word & CANOPEN_STATUS_HOMED);
}

/* =========================================================================
 * Group 5: Control word processing
 * ========================================================================= */

void test_ctrl_enable_causes_tmc5160_enable(void)
{
    /* CTRL_ENABLE should call tmc5160_enable() which writes CHOPCONF */
    canopen_stepper_rpdo_write(0, 80000, CANOPEN_CTRL_ENABLE, TMC5160_RAMPMODE_POSITION);
    canopen_sync_process();

    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, find_write(TMC5160_REG_CHOPCONF),
                                  "CHOPCONF not written (tmc5160_enable not called)");
}

void test_ctrl_halt_writes_zero_velocity(void)
{
    /* CTRL_HALT must override the staged max_vel with 0 */
    canopen_stepper_rpdo_write(5000, 80000, CANOPEN_CTRL_HALT, TMC5160_RAMPMODE_VEL_POS);
    canopen_sync_process();

    int idx = find_write(TMC5160_REG_VMAX);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, idx, "VMAX not written");
    TEST_ASSERT_EQUAL_UINT32(0, logged_data(idx));
}

void test_ctrl_clear_fault_clears_fault_bit(void)
{
    canopen_set_status_bit(CANOPEN_STATUS_FAULT);

    canopen_stepper_rpdo_write(0, 0, CANOPEN_CTRL_CLEAR_FAULT, TMC5160_RAMPMODE_POSITION);
    canopen_sync_process();

    TEST_ASSERT_FALSE(canopen_get_status_word() & CANOPEN_STATUS_FAULT);
}

void test_ctrl_clear_fault_does_not_affect_other_bits(void)
{
    canopen_set_status_bit(CANOPEN_STATUS_FAULT | CANOPEN_STATUS_HOMED);

    canopen_stepper_rpdo_write(0, 0, CANOPEN_CTRL_CLEAR_FAULT, TMC5160_RAMPMODE_POSITION);
    canopen_sync_process();

    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_HOMED);
}

/* =========================================================================
 * Group 6: Status word API
 * ========================================================================= */

void test_status_word_initially_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, canopen_get_status_word());
}

void test_set_status_bit_sets_bit(void)
{
    canopen_set_status_bit(CANOPEN_STATUS_HOMED);

    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_HOMED);
}

void test_set_status_bit_does_not_clear_others(void)
{
    canopen_set_status_bit(CANOPEN_STATUS_HOMED);
    canopen_set_status_bit(CANOPEN_STATUS_FAULT);

    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_HOMED);
    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_FAULT);
}

void test_clear_status_bit_clears_bit(void)
{
    canopen_set_status_bit(CANOPEN_STATUS_HOMED);
    canopen_clear_status_bit(CANOPEN_STATUS_HOMED);

    TEST_ASSERT_FALSE(canopen_get_status_word() & CANOPEN_STATUS_HOMED);
}

void test_clear_status_bit_does_not_affect_others(void)
{
    canopen_set_status_bit(CANOPEN_STATUS_HOMED | CANOPEN_STATUS_FAULT);
    canopen_clear_status_bit(CANOPEN_STATUS_FAULT);

    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_HOMED);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* RPDO staging */
    RUN_TEST(test_rpdo_staging_does_not_trigger_spi);
    RUN_TEST(test_rpdo_staging_second_write_overwrites_first);

    /* SYNC application */
    RUN_TEST(test_sync_writes_xtarget_from_staged_rpdo);
    RUN_TEST(test_sync_writes_negative_xtarget);
    RUN_TEST(test_sync_writes_vmax_from_staged_rpdo);
    RUN_TEST(test_sync_writes_rampmode_from_staged_rpdo);
    RUN_TEST(test_sync_without_pending_rpdo_does_not_write_xtarget);
    RUN_TEST(test_sync_clears_pending_second_sync_does_not_reapply);

    /* TPDO latching */
    RUN_TEST(test_tpdo_latches_actual_position);
    RUN_TEST(test_tpdo_latches_negative_position);
    RUN_TEST(test_tpdo_latches_ramp_status_byte);
    RUN_TEST(test_tpdo_latches_tof_distance);
    RUN_TEST(test_tpdo_tof_distance_zero_by_default);

    /* Status word from RAMPSTAT */
    RUN_TEST(test_status_moving_set_when_vzero_clear);
    RUN_TEST(test_status_moving_clear_when_vzero_set);
    RUN_TEST(test_status_in_position_set_when_pos_reached);
    RUN_TEST(test_status_in_position_clear_when_pos_not_reached);
    RUN_TEST(test_tpdo_status_word_captures_pre_sync_status);

    /* Control word */
    RUN_TEST(test_ctrl_enable_causes_tmc5160_enable);
    RUN_TEST(test_ctrl_halt_writes_zero_velocity);
    RUN_TEST(test_ctrl_clear_fault_clears_fault_bit);
    RUN_TEST(test_ctrl_clear_fault_does_not_affect_other_bits);

    /* Status word API */
    RUN_TEST(test_status_word_initially_zero);
    RUN_TEST(test_set_status_bit_sets_bit);
    RUN_TEST(test_set_status_bit_does_not_clear_others);
    RUN_TEST(test_clear_status_bit_clears_bit);
    RUN_TEST(test_clear_status_bit_does_not_affect_others);

    return UNITY_END();
}
