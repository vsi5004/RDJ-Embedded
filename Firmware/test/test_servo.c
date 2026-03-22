/* Phase 5 — RC servo PWM driver tests
 *
 * Tests cover:
 *   - Pass-through: valid pulse widths in [500, 2500] μs reach the PWM HAL
 *     unchanged, on the correct channel
 *   - Clamping low: requests below 500 μs are clamped to 500 μs
 *   - Clamping high: requests above 2500 μs are clamped to 2500 μs
 *   - Boundary values: exactly 500 and 2500 μs are not modified
 *
 * Each test calls setUp() which resets the PWM mock and re-inits the driver.
 *
 * Naming convention: test_servo_<scenario>
 */

#include "unity.h"
#include "drv_servo.h"
#include "mock_pwm.h"

void setUp(void)
{
    mock_pwm_reset();
    drv_servo_init(&mock_hal_pwm);
}

void tearDown(void) {}

/* =========================================================================
 * Group 1: pass-through — valid values forwarded without change
 * ========================================================================= */

void test_servo_set_forwards_pulse_to_pwm(void)
{
    drv_servo_set(1, 1500);

    TEST_ASSERT_EQUAL_UINT32(1500, mock_pwm_last_pulse_us);
}

void test_servo_set_forwards_channel_to_pwm(void)
{
    drv_servo_set(1, 1500);

    TEST_ASSERT_EQUAL_UINT8(1, mock_pwm_last_channel);
}

void test_servo_set_channel_2(void)
{
    drv_servo_set(2, 1800);

    TEST_ASSERT_EQUAL_UINT8(2, mock_pwm_last_channel);
    TEST_ASSERT_EQUAL_UINT32(1800, mock_pwm_last_pulse_us);
}

void test_servo_set_calls_pwm_once(void)
{
    drv_servo_set(1, 1500);

    TEST_ASSERT_EQUAL_INT(1, mock_pwm_call_count);
}

/* =========================================================================
 * Group 2: clamping — out-of-range values are constrained
 * ========================================================================= */

void test_servo_clamps_zero_to_min(void)
{
    drv_servo_set(1, 0);

    TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MIN_US, mock_pwm_last_pulse_us);
}

void test_servo_clamps_below_min(void)
{
    drv_servo_set(1, 100);

    TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MIN_US, mock_pwm_last_pulse_us);
}

void test_servo_clamps_above_max(void)
{
    drv_servo_set(1, 5000);

    TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MAX_US, mock_pwm_last_pulse_us);
}

/* =========================================================================
 * Group 3: boundary values — min and max are accepted unchanged
 * ========================================================================= */

void test_servo_allows_min_boundary(void)
{
    drv_servo_set(1, SERVO_PULSE_MIN_US);

    TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MIN_US, mock_pwm_last_pulse_us);
}

void test_servo_allows_max_boundary(void)
{
    drv_servo_set(1, SERVO_PULSE_MAX_US);

    TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MAX_US, mock_pwm_last_pulse_us);
}

void test_servo_centre_position_unchanged(void)
{
    drv_servo_set(1, 1500);  /* nominal centre */

    TEST_ASSERT_EQUAL_UINT32(1500, mock_pwm_last_pulse_us);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* pass-through */
    RUN_TEST(test_servo_set_forwards_pulse_to_pwm);
    RUN_TEST(test_servo_set_forwards_channel_to_pwm);
    RUN_TEST(test_servo_set_channel_2);
    RUN_TEST(test_servo_set_calls_pwm_once);

    /* clamping */
    RUN_TEST(test_servo_clamps_zero_to_min);
    RUN_TEST(test_servo_clamps_below_min);
    RUN_TEST(test_servo_clamps_above_max);

    /* boundaries */
    RUN_TEST(test_servo_allows_min_boundary);
    RUN_TEST(test_servo_allows_max_boundary);
    RUN_TEST(test_servo_centre_position_unchanged);

    return UNITY_END();
}
