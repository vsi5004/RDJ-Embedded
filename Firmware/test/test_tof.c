/* Phase 5 — VL53L4CD Time-of-Flight driver tests
 *
 * Tests cover:
 *   - init(): writes to the correct I2C address
 *   - start_ranging(): writes SYSTEM__START register (0x0087) with 0x40
 *   - is_data_ready(): reads GPIO__TIO_HV_STATUS (0x0031);
 *       returns true when bit 0 == 0, false when bit 0 == 1
 *   - get_distance_mm(): reads 2 bytes from RANGE_MM register (0x0096);
 *       combines [hi, lo] as (hi << 8) | lo
 *   - clear_interrupt(): writes 0x01 to SYSTEM__INTERRUPT_CLEAR (0x0086)
 *
 * All VL53 registers use 16-bit addresses, sent MSB-first as the first two
 * bytes of each I2C write.  The mock I2C records every write; write_read()
 * consumes two log slots (write at N, read at N+1).
 *
 * setUp() calls drv_tof_init() then resets the I2C log so each test starts
 * with a clean record.
 *
 * Naming convention: test_tof_<scenario>
 */

#include "unity.h"
#include "drv_tof.h"
#include "mock_i2c.h"

void setUp(void)
{
    mock_i2c_reset();
    drv_tof_init(&mock_hal_i2c);
    mock_i2c_reset();  /* clear init traffic */
}

void tearDown(void) {}

/* =========================================================================
 * Group 1: init() — uses the correct I2C address
 * ========================================================================= */

void test_tof_init_uses_correct_i2c_address(void)
{
    drv_tof_init(&mock_hal_i2c);  /* call explicitly so log is fresh */

    TEST_ASSERT_EQUAL_UINT8(TOF_I2C_ADDR, mock_i2c_log[0].addr);
}

/* =========================================================================
 * Group 2: start_ranging() — writes SYSTEM__START with 0x40
 * ========================================================================= */

void test_tof_start_ranging_uses_correct_i2c_address(void)
{
    drv_tof_start_ranging();

    TEST_ASSERT_EQUAL_UINT8(TOF_I2C_ADDR, mock_i2c_log[0].addr);
}

void test_tof_start_ranging_writes_system_start_register(void)
{
    drv_tof_start_ranging();

    /* Register 0x0087: addr bytes [0x00, 0x87], value byte 0x40 */
    TEST_ASSERT_EQUAL_UINT8(0x00, mock_i2c_log[0].data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x87, mock_i2c_log[0].data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x40, mock_i2c_log[0].data[2]);
}

/* =========================================================================
 * Group 3: is_data_ready() — reads STATUS register and interprets bit 0
 * ========================================================================= */

void test_tof_is_data_ready_reads_status_register(void)
{
    drv_tof_is_data_ready();

    /* write_read: write part at log[0] carries register address [0x00, 0x31] */
    TEST_ASSERT_EQUAL_UINT8(0x00, mock_i2c_log[0].data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x31, mock_i2c_log[0].data[1]);
}

void test_tof_is_data_ready_true_when_bit0_clear(void)
{
    /* write_read: write=slot 0, read=slot 1; program slot 1 response */
    uint8_t status = 0x00;  /* bit 0 clear → data ready */
    mock_i2c_program_read_response(1, &status, 1);

    TEST_ASSERT_TRUE(drv_tof_is_data_ready());
}

void test_tof_is_data_ready_false_when_bit0_set(void)
{
    uint8_t status = 0x01;  /* bit 0 set → not ready */
    mock_i2c_program_read_response(1, &status, 1);

    TEST_ASSERT_FALSE(drv_tof_is_data_ready());
}

void test_tof_is_data_ready_ignores_upper_bits(void)
{
    /* Only bit 0 matters; upper bits set but bit 0 clear → still ready */
    uint8_t status = 0xFE;  /* bit 0 clear, all others set */
    mock_i2c_program_read_response(1, &status, 1);

    TEST_ASSERT_TRUE(drv_tof_is_data_ready());
}

/* =========================================================================
 * Group 4: get_distance_mm() — reads RANGE_MM and assembles uint16
 * ========================================================================= */

void test_tof_get_distance_reads_range_register(void)
{
    drv_tof_get_distance_mm();

    /* write_read write part: register 0x0096 → [0x00, 0x96] */
    TEST_ASSERT_EQUAL_UINT8(0x00, mock_i2c_log[0].data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x96, mock_i2c_log[0].data[1]);
}

void test_tof_get_distance_combines_bytes_big_endian(void)
{
    uint8_t resp[2] = {0x01, 0x90};  /* big-endian 0x0190 = 400 mm */
    mock_i2c_program_read_response(1, resp, 2);

    TEST_ASSERT_EQUAL_UINT16(400, drv_tof_get_distance_mm());
}

void test_tof_get_distance_zero(void)
{
    uint8_t resp[2] = {0x00, 0x00};
    mock_i2c_program_read_response(1, resp, 2);

    TEST_ASSERT_EQUAL_UINT16(0, drv_tof_get_distance_mm());
}

void test_tof_get_distance_max_range(void)
{
    /* VL53L1X max ~4000 mm = 0x0FA0 */
    uint8_t resp[2] = {0x0F, 0xA0};
    mock_i2c_program_read_response(1, resp, 2);

    TEST_ASSERT_EQUAL_UINT16(4000, drv_tof_get_distance_mm());
}

/* =========================================================================
 * Group 5: clear_interrupt() — writes SYSTEM__INTERRUPT_CLEAR with 0x01
 * ========================================================================= */

void test_tof_clear_interrupt_uses_correct_i2c_address(void)
{
    drv_tof_clear_interrupt();

    TEST_ASSERT_EQUAL_UINT8(TOF_I2C_ADDR, mock_i2c_log[0].addr);
}

void test_tof_clear_interrupt_writes_correct_register_and_value(void)
{
    drv_tof_clear_interrupt();

    /* Register 0x0086, value 0x01 */
    TEST_ASSERT_EQUAL_UINT8(0x00, mock_i2c_log[0].data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x86, mock_i2c_log[0].data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x01, mock_i2c_log[0].data[2]);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* init() */
    RUN_TEST(test_tof_init_uses_correct_i2c_address);

    /* start_ranging() */
    RUN_TEST(test_tof_start_ranging_uses_correct_i2c_address);
    RUN_TEST(test_tof_start_ranging_writes_system_start_register);

    /* is_data_ready() */
    RUN_TEST(test_tof_is_data_ready_reads_status_register);
    RUN_TEST(test_tof_is_data_ready_true_when_bit0_clear);
    RUN_TEST(test_tof_is_data_ready_false_when_bit0_set);
    RUN_TEST(test_tof_is_data_ready_ignores_upper_bits);

    /* get_distance_mm() */
    RUN_TEST(test_tof_get_distance_reads_range_register);
    RUN_TEST(test_tof_get_distance_combines_bytes_big_endian);
    RUN_TEST(test_tof_get_distance_zero);
    RUN_TEST(test_tof_get_distance_max_range);

    /* clear_interrupt() */
    RUN_TEST(test_tof_clear_interrupt_uses_correct_i2c_address);
    RUN_TEST(test_tof_clear_interrupt_writes_correct_register_and_value);

    return UNITY_END();
}
