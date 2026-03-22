/* Fault detection, EMCY generation, and motion blocking tests
 *
 * Tests cover:
 *   - report_fault(): sets FAULT status bit and transmits an EMCY frame
 *   - EMCY frame format: correct CAN ID, error code (little-endian),
 *     error register, and length
 *   - Fault blocks motion: RPDO motion commands ignored while FAULT is set;
 *     TPDO latching continues normally
 *   - Clear-fault recovery: CTRL_CLEAR_FAULT in control word clears the fault
 *     bit and unblocks subsequent motion commands
 *   - Multiple distinct EMCY codes: StallGuard (0x2100), unexpected endstop
 *     (0x2200)
 *
 * setUp() initialises TMC5160, CAN mock, and CANopen fresh for each test
 * so there are no inter-test dependencies.
 *
 * Naming convention: test_fault_<scenario>
 */

#include "unity.h"
#include "app_canopen.h"
#include "drv_tmc5160.h"
#include "mock_spi.h"
#include "mock_can.h"

/* -------------------------------------------------------------------------
 * Test fixture
 * ------------------------------------------------------------------------- */

#define TEST_NODE_ID  1u

static const tmc5160_config_t CFG = {
    .toff = 5, .irun = 20, .ihold = 8, .iholddelay = 6,
    .vmax = 100000, .amax = 500, .dmax = 500, .sgt = 10,
};

void setUp(void)
{
    mock_spi_reset();
    mock_can_reset();
    tmc5160_init(&mock_hal_spi, &CFG);
    mock_spi_reset();  /* clear init traffic */
    canopen_init();
    canopen_bind_can(&mock_hal_can, TEST_NODE_ID);
}

void tearDown(void) {}

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static uint8_t  logged_reg(int i)      { return mock_spi_log[i].tx[0] & 0x7Fu; }
static int      logged_is_write(int i) { return (mock_spi_log[i].tx[0] & 0x80u) != 0; }

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

/* canopen_sync_process() reads RAMPSTAT then XACTUAL.
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
 * Group 1: report_fault() — sets status bit and transmits EMCY frame
 * ========================================================================= */

void test_fault_report_sets_fault_status_bit(void)
{
    canopen_report_fault(0x2100);

    TEST_ASSERT_TRUE(canopen_get_status_word() & CANOPEN_STATUS_FAULT);
}

void test_fault_report_sends_emcy_frame(void)
{
    canopen_report_fault(0x2100);

    TEST_ASSERT_EQUAL_INT(1, mock_can_tx_count);
}

void test_fault_emcy_has_correct_can_id(void)
{
    canopen_report_fault(0x2100);

    TEST_ASSERT_EQUAL_UINT32(0x80u + TEST_NODE_ID, mock_can_tx_log[0].id);
}

void test_fault_emcy_error_code_is_little_endian(void)
{
    canopen_report_fault(0x2100);

    /* CANopen EMCY: bytes 0-1 = error code, little-endian */
    TEST_ASSERT_EQUAL_HEX8(0x00, mock_can_tx_log[0].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x21, mock_can_tx_log[0].data[1]);
}

void test_fault_emcy_error_register_is_generic_error(void)
{
    canopen_report_fault(0x2100);

    /* CANopen EMCY: byte 2 = error register (OD 0x1001). bit 0 = generic error. */
    TEST_ASSERT_EQUAL_HEX8(0x01, mock_can_tx_log[0].data[2]);
}

void test_fault_emcy_has_correct_length(void)
{
    canopen_report_fault(0x2100);

    TEST_ASSERT_EQUAL_UINT8(8, mock_can_tx_log[0].len);
}

void test_fault_second_report_sends_second_emcy(void)
{
    canopen_report_fault(0x2100);
    canopen_report_fault(0x2200);

    TEST_ASSERT_EQUAL_INT(2, mock_can_tx_count);
}

/* =========================================================================
 * Group 2: fault blocks motion, but does not block TPDO latching
 * ========================================================================= */

void test_fault_blocks_rpdo_motion_commands(void)
{
    canopen_stepper_rpdo_write(5000, 80000, CANOPEN_CTRL_ENABLE,
                               TMC5160_RAMPMODE_POSITION);
    canopen_report_fault(0x2100);
    program_sync_reads(0, 0);

    canopen_sync_process();

    /* No motion registers should be written to TMC5160 */
    TEST_ASSERT_EQUAL_INT(-1, find_write(TMC5160_REG_XTARGET));
    TEST_ASSERT_EQUAL_INT(-1, find_write(TMC5160_REG_VMAX));
}

void test_fault_does_not_block_tpdo_latching(void)
{
    canopen_report_fault(0x2100);
    program_sync_reads(0, 12345);

    canopen_sync_process();

    TEST_ASSERT_EQUAL_INT32(12345, canopen_get_tpdo_stepper()->actual_position);
}

void test_fault_pending_rpdo_is_consumed_even_while_faulted(void)
{
    /* Stage an RPDO, fault, SYNC — pending should be consumed so a second
     * SYNC without a new RPDO does not spuriously apply stale commands. */
    canopen_stepper_rpdo_write(5000, 80000, 0, TMC5160_RAMPMODE_POSITION);
    canopen_report_fault(0x2100);
    program_sync_reads(0, 0);
    canopen_sync_process();

    mock_spi_reset();
    program_sync_reads(0, 0);
    canopen_sync_process();

    TEST_ASSERT_EQUAL_INT(-1, find_write(TMC5160_REG_XTARGET));
}

/* =========================================================================
 * Group 3: clear-fault recovery
 * ========================================================================= */

void test_fault_clear_clears_fault_bit(void)
{
    canopen_report_fault(0x2100);
    canopen_stepper_rpdo_write(0, 0, CANOPEN_CTRL_CLEAR_FAULT, 0);
    program_sync_reads(0, 0);
    canopen_sync_process();

    TEST_ASSERT_FALSE(canopen_get_status_word() & CANOPEN_STATUS_FAULT);
}

void test_fault_clear_unblocks_subsequent_motion(void)
{
    /* Fault, then clear, then send a motion RPDO — it must be applied. */
    canopen_report_fault(0x2100);

    canopen_stepper_rpdo_write(0, 0, CANOPEN_CTRL_CLEAR_FAULT, 0);
    program_sync_reads(0, 0);
    canopen_sync_process();  /* clears fault */

    mock_spi_reset();
    canopen_stepper_rpdo_write(9000, 50000, 0, TMC5160_RAMPMODE_POSITION);
    program_sync_reads(0, 0);
    canopen_sync_process();  /* motion should now apply */

    TEST_ASSERT_NOT_EQUAL(-1, find_write(TMC5160_REG_XTARGET));
}

/* =========================================================================
 * Group 4: distinct EMCY error codes
 * ========================================================================= */

void test_fault_stallguard_emcy_code(void)
{
    canopen_report_fault(0x2100);

    TEST_ASSERT_EQUAL_HEX8(0x00, mock_can_tx_log[0].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x21, mock_can_tx_log[0].data[1]);
}

void test_fault_unexpected_endstop_emcy_code(void)
{
    canopen_report_fault(0x2200);

    TEST_ASSERT_EQUAL_HEX8(0x00, mock_can_tx_log[0].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22, mock_can_tx_log[0].data[1]);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* report_fault() */
    RUN_TEST(test_fault_report_sets_fault_status_bit);
    RUN_TEST(test_fault_report_sends_emcy_frame);
    RUN_TEST(test_fault_emcy_has_correct_can_id);
    RUN_TEST(test_fault_emcy_error_code_is_little_endian);
    RUN_TEST(test_fault_emcy_error_register_is_generic_error);
    RUN_TEST(test_fault_emcy_has_correct_length);
    RUN_TEST(test_fault_second_report_sends_second_emcy);

    /* fault blocks motion */
    RUN_TEST(test_fault_blocks_rpdo_motion_commands);
    RUN_TEST(test_fault_does_not_block_tpdo_latching);
    RUN_TEST(test_fault_pending_rpdo_is_consumed_even_while_faulted);

    /* clear-fault recovery */
    RUN_TEST(test_fault_clear_clears_fault_bit);
    RUN_TEST(test_fault_clear_unblocks_subsequent_motion);

    /* distinct EMCY codes */
    RUN_TEST(test_fault_stallguard_emcy_code);
    RUN_TEST(test_fault_unexpected_endstop_emcy_code);

    return UNITY_END();
}
