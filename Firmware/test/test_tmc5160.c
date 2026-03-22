/* Phase 1 — TMC5160 SPI driver tests
 *
 * Tests are independent: each calls mock_spi_reset() in setUp().
 * No test relies on side-effects from another.
 *
 * Naming convention: test_<function>_<scenario>
 */

#include "unity.h"
#include "drv_tmc5160.h"
#include "mock_spi.h"

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static const tmc5160_config_t DEFAULT_CFG = {
    .toff       = 5,
    .irun       = 20,
    .ihold      = 8,
    .iholddelay = 6,
    .vmax       = 100000,
    .amax       = 500,
    .dmax       = 500,
    .sgt        = 10,
};

static void do_init(void)
{
    tmc5160_init(&mock_hal_spi, &DEFAULT_CFG);
}

/* Extract the 32-bit data value from a logged write transaction at index i */
static uint32_t logged_data(int i)
{
    uint8_t *b = mock_spi_log[i].tx;
    return ((uint32_t)b[1] << 24)
         | ((uint32_t)b[2] << 16)
         | ((uint32_t)b[3] <<  8)
         | ((uint32_t)b[4]);
}

/* Extract the register address byte (bit 7 cleared) from log entry i */
static uint8_t logged_reg(int i)
{
    return mock_spi_log[i].tx[0] & 0x7Fu;
}

/* Check whether log entry i was a write (bit 7 of byte 0 set) */
static int logged_is_write(int i)
{
    return (mock_spi_log[i].tx[0] & 0x80u) != 0;
}

/* -------------------------------------------------------------------------
 * setUp / tearDown
 * ------------------------------------------------------------------------- */

void setUp(void)    { mock_spi_reset(); }
void tearDown(void) {}

/* -------------------------------------------------------------------------
 * Raw register access
 * ------------------------------------------------------------------------- */

void test_write_sets_write_bit_in_first_byte(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_write(0x2D, 0x12345678);

    TEST_ASSERT_EQUAL_INT(1, mock_spi_call_count);
    TEST_ASSERT_EQUAL_HEX8(0x2D | 0x80, mock_spi_log[0].tx[0]);
}

void test_write_packs_data_msb_first(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_write(0x2D, 0xDEADBEEF);

    uint8_t *b = mock_spi_log[0].tx;
    TEST_ASSERT_EQUAL_HEX8(0xDE, b[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, b[2]);
    TEST_ASSERT_EQUAL_HEX8(0xBE, b[3]);
    TEST_ASSERT_EQUAL_HEX8(0xEF, b[4]);
}

void test_write_transfer_is_five_bytes(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_write(0x00, 0);

    TEST_ASSERT_EQUAL_UINT16(5, mock_spi_log[0].len);
}

void test_read_performs_two_spi_transfers(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_read(TMC5160_REG_XACTUAL);

    TEST_ASSERT_EQUAL_INT(2, mock_spi_call_count);
}

void test_read_sends_address_without_write_bit(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_read(TMC5160_REG_XACTUAL);

    /* Both transactions must have bit 7 clear (read direction) */
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_XACTUAL, mock_spi_log[0].tx[0]);
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_XACTUAL, mock_spi_log[1].tx[0]);
}

void test_read_returns_value_from_second_transfer(void)
{
    do_init();
    mock_spi_reset();

    /* Program second transfer (index 1) to return 0x00AABBCC */
    uint8_t resp[5] = { 0x00, 0x00, 0xAA, 0xBB, 0xCC };
    mock_spi_program_response(1, resp, 5);

    uint32_t result = tmc5160_read(TMC5160_REG_XACTUAL);

    TEST_ASSERT_EQUAL_HEX32(0x00AABBCC, result);
}

/* -------------------------------------------------------------------------
 * Initialisation sequence
 * ------------------------------------------------------------------------- */

void test_init_writes_gconf_first(void)
{
    do_init();

    TEST_ASSERT_TRUE(logged_is_write(0));
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_GCONF, logged_reg(0));
    TEST_ASSERT_EQUAL_HEX32(0x00000000, logged_data(0));
}

void test_init_writes_chopconf(void)
{
    do_init();

    /* Second write goes to CHOPCONF */
    TEST_ASSERT_TRUE(logged_is_write(1));
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_CHOPCONF, logged_reg(1));

    /* TOFF bits [3:0] must match the configured value */
    uint32_t chopconf = logged_data(1);
    TEST_ASSERT_EQUAL_UINT32(DEFAULT_CFG.toff, chopconf & 0x0Fu);
}

void test_init_writes_ihold_irun(void)
{
    do_init();

    TEST_ASSERT_TRUE(logged_is_write(2));
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_IHOLD_IRUN, logged_reg(2));

    uint32_t val = logged_data(2);
    TEST_ASSERT_EQUAL_UINT32(DEFAULT_CFG.ihold,      val & 0x1Fu);
    TEST_ASSERT_EQUAL_UINT32(DEFAULT_CFG.irun,       (val >> 8)  & 0x1Fu);
    TEST_ASSERT_EQUAL_UINT32(DEFAULT_CFG.iholddelay, (val >> 16) & 0x0Fu);
}

void test_init_writes_swmode_with_endstop_and_stallguard(void)
{
    do_init();

    TEST_ASSERT_TRUE(logged_is_write(3));
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_SWMODE, logged_reg(3));

    uint32_t val = logged_data(3);
    TEST_ASSERT_TRUE(val & TMC5160_SWMODE_STOP_L_ENABLE);
    TEST_ASSERT_TRUE(val & TMC5160_SWMODE_SG_STOP);
}

void test_init_writes_coolconf_with_sgt(void)
{
    do_init();

    TEST_ASSERT_TRUE(logged_is_write(4));
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_COOLCONF, logged_reg(4));

    uint32_t val = logged_data(4);
    int8_t sgt_readback = (int8_t)((val >> 24) & 0x7Fu);
    TEST_ASSERT_EQUAL_INT8(DEFAULT_CFG.sgt, sgt_readback);
}

void test_init_writes_vmax(void)
{
    do_init();

    /* VMAX is written after VSTART and before AMAX/DMAX.
     * Scan for the VMAX write rather than assuming a fixed index. */
    int found = 0;
    for (int i = 0; i < mock_spi_call_count; i++) {
        if (logged_is_write(i) && logged_reg(i) == TMC5160_REG_VMAX) {
            TEST_ASSERT_EQUAL_HEX32(DEFAULT_CFG.vmax, logged_data(i));
            found = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "VMAX register not written during init");
}

void test_init_sets_rampmode_position(void)
{
    do_init();

    int found = 0;
    for (int i = 0; i < mock_spi_call_count; i++) {
        if (logged_is_write(i) && logged_reg(i) == TMC5160_REG_RAMPMODE) {
            TEST_ASSERT_EQUAL_HEX32(TMC5160_RAMPMODE_POSITION, logged_data(i));
            found = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "RAMPMODE register not written during init");
}

void test_init_zeros_xactual(void)
{
    do_init();

    int found = 0;
    for (int i = 0; i < mock_spi_call_count; i++) {
        if (logged_is_write(i) && logged_reg(i) == TMC5160_REG_XACTUAL) {
            TEST_ASSERT_EQUAL_HEX32(0, logged_data(i));
            found = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "XACTUAL not zeroed during init");
}

/* -------------------------------------------------------------------------
 * Motion control
 * ------------------------------------------------------------------------- */

void test_set_target_writes_xtarget(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_set_target(12345);

    TEST_ASSERT_EQUAL_INT(1, mock_spi_call_count);
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_XTARGET, logged_reg(0));
    TEST_ASSERT_EQUAL_HEX32((uint32_t)12345, logged_data(0));
}

void test_set_target_handles_negative_position(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_set_target(-1000);

    TEST_ASSERT_EQUAL_HEX32((uint32_t)-1000, logged_data(0));
}

void test_set_velocity_writes_vmax(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_set_velocity(200000);

    TEST_ASSERT_EQUAL_INT(1, mock_spi_call_count);
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_VMAX, logged_reg(0));
    TEST_ASSERT_EQUAL_HEX32(200000, logged_data(0));
}

void test_set_accel_writes_amax_then_dmax(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_set_accel(1000, 800);

    TEST_ASSERT_EQUAL_INT(2, mock_spi_call_count);
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_AMAX, logged_reg(0));
    TEST_ASSERT_EQUAL_HEX32(1000, logged_data(0));
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_DMAX, logged_reg(1));
    TEST_ASSERT_EQUAL_HEX32(800, logged_data(1));
}

void test_set_rampmode_writes_register(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_set_rampmode(TMC5160_RAMPMODE_VEL_POS);

    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_RAMPMODE, logged_reg(0));
    TEST_ASSERT_EQUAL_HEX32(TMC5160_RAMPMODE_VEL_POS, logged_data(0));
}

/* -------------------------------------------------------------------------
 * Status readback
 * ------------------------------------------------------------------------- */

void test_get_position_reads_xactual(void)
{
    do_init();
    mock_spi_reset();

    uint8_t resp[5] = { 0x00, 0x00, 0x00, 0x30, 0x39 };  /* 0x3039 = 12345 */
    mock_spi_program_response(1, resp, 5);

    int32_t pos = tmc5160_get_position();

    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_XACTUAL, mock_spi_log[0].tx[0] & 0x7F);
    TEST_ASSERT_EQUAL_INT32(12345, pos);
}

void test_get_rampstat_reads_rampstat_register(void)
{
    do_init();
    mock_spi_reset();

    uint8_t resp[5] = { 0x00, 0x00, 0x00, 0x02, 0x00 };  /* position_reached bit */
    mock_spi_program_response(1, resp, 5);

    uint32_t rs = tmc5160_get_rampstat();

    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_RAMPSTAT, mock_spi_log[0].tx[0] & 0x7F);
    TEST_ASSERT_EQUAL_HEX32(0x0200, rs);
}

void test_get_stallguard_reads_drv_status_bits_9_to_0(void)
{
    do_init();
    mock_spi_reset();

    /* DRV_STATUS with SG_RESULT = 0x1FF (all 9 bits set) */
    uint8_t resp[5] = { 0x00, 0x00, 0x00, 0x01, 0xFF };
    mock_spi_program_response(1, resp, 5);

    uint16_t sg = tmc5160_get_stallguard();

    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_DRV_STATUS, mock_spi_log[0].tx[0] & 0x7F);
    TEST_ASSERT_EQUAL_HEX16(0x01FF, sg);
}

void test_get_stallguard_masks_upper_bits(void)
{
    do_init();
    mock_spi_reset();

    /* Set bits above 9 in DRV_STATUS — should be masked out */
    uint8_t resp[5] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF };
    mock_spi_program_response(1, resp, 5);

    uint16_t sg = tmc5160_get_stallguard();

    TEST_ASSERT_EQUAL_HEX16(0x03FF, sg);
}

/* -------------------------------------------------------------------------
 * Enable / disable
 * ------------------------------------------------------------------------- */

void test_disable_clears_toff_in_chopconf(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_disable();

    TEST_ASSERT_EQUAL_INT(1, mock_spi_call_count);
    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_CHOPCONF, logged_reg(0));
    /* TOFF bits [3:0] must be zero */
    TEST_ASSERT_EQUAL_UINT32(0, logged_data(0) & 0x0Fu);
}

void test_enable_restores_toff_in_chopconf(void)
{
    do_init();
    mock_spi_reset();

    tmc5160_disable();
    mock_spi_reset();
    tmc5160_enable();

    TEST_ASSERT_EQUAL_HEX8(TMC5160_REG_CHOPCONF, logged_reg(0));
    /* TOFF bits must match the configured value again */
    TEST_ASSERT_EQUAL_UINT32(DEFAULT_CFG.toff, logged_data(0) & 0x0Fu);
}

void test_enable_after_disable_does_not_change_other_chopconf_bits(void)
{
    do_init();

    /* Capture CHOPCONF written during init */
    uint32_t init_chopconf = 0;
    for (int i = 0; i < mock_spi_call_count; i++) {
        if (logged_is_write(i) && logged_reg(i) == TMC5160_REG_CHOPCONF) {
            init_chopconf = logged_data(i);
            break;
        }
    }

    mock_spi_reset();
    tmc5160_disable();
    mock_spi_reset();
    tmc5160_enable();

    uint32_t re_enabled = logged_data(0);
    TEST_ASSERT_EQUAL_HEX32(init_chopconf, re_enabled);
}

/* -------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_write_sets_write_bit_in_first_byte);
    RUN_TEST(test_write_packs_data_msb_first);
    RUN_TEST(test_write_transfer_is_five_bytes);
    RUN_TEST(test_read_performs_two_spi_transfers);
    RUN_TEST(test_read_sends_address_without_write_bit);
    RUN_TEST(test_read_returns_value_from_second_transfer);

    RUN_TEST(test_init_writes_gconf_first);
    RUN_TEST(test_init_writes_chopconf);
    RUN_TEST(test_init_writes_ihold_irun);
    RUN_TEST(test_init_writes_swmode_with_endstop_and_stallguard);
    RUN_TEST(test_init_writes_coolconf_with_sgt);
    RUN_TEST(test_init_writes_vmax);
    RUN_TEST(test_init_sets_rampmode_position);
    RUN_TEST(test_init_zeros_xactual);

    RUN_TEST(test_set_target_writes_xtarget);
    RUN_TEST(test_set_target_handles_negative_position);
    RUN_TEST(test_set_velocity_writes_vmax);
    RUN_TEST(test_set_accel_writes_amax_then_dmax);
    RUN_TEST(test_set_rampmode_writes_register);

    RUN_TEST(test_get_position_reads_xactual);
    RUN_TEST(test_get_rampstat_reads_rampstat_register);
    RUN_TEST(test_get_stallguard_reads_drv_status_bits_9_to_0);
    RUN_TEST(test_get_stallguard_masks_upper_bits);

    RUN_TEST(test_disable_clears_toff_in_chopconf);
    RUN_TEST(test_enable_restores_toff_in_chopconf);
    RUN_TEST(test_enable_after_disable_does_not_change_other_chopconf_bits);

    return UNITY_END();
}
