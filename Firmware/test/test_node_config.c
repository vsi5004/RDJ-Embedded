/* Phase 6a — Node configuration macro tests
 *
 * Verifies that each NODE_HAS_* macro returns the correct value for every
 * valid node ID (1–5), based on the population matrix in PCB_DESIGN.md:
 *
 *   ID | Board         | TMC | Servo | ToF | Pot | LED
 *   ---+---------------+-----+-------+-----+-----+----
 *   1  | X axis        |  ●  |       |  ●  |     |
 *   2  | Z axis        |  ●  |       |  ●  |     |
 *   3  | A axis        |  ●  |       |  ●  |  ●  |  ●
 *   4  | Pincher       |     |   ●   |  ●  |     |
 *   5  | Player        |     |   ●   |     |     |  ●
 *
 * No setUp/tearDown needed — the macros have no runtime state.
 *
 * Naming convention: test_<macro>_<node_role>
 */

#include "unity.h"
#include "node_config.h"

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * NODE_HAS_TMC — stepper nodes 1, 2, 3 only
 * ========================================================================= */

void test_node_has_tmc_x_axis(void)      { TEST_ASSERT_TRUE (NODE_HAS_TMC(1)); }
void test_node_has_tmc_z_axis(void)      { TEST_ASSERT_TRUE (NODE_HAS_TMC(2)); }
void test_node_has_tmc_a_axis(void)      { TEST_ASSERT_TRUE (NODE_HAS_TMC(3)); }
void test_node_has_tmc_pincher(void)     { TEST_ASSERT_FALSE(NODE_HAS_TMC(4)); }
void test_node_has_tmc_player(void)      { TEST_ASSERT_FALSE(NODE_HAS_TMC(5)); }

/* =========================================================================
 * NODE_HAS_SERVO — servo nodes 4, 5 only
 * ========================================================================= */

void test_node_has_servo_x_axis(void)    { TEST_ASSERT_FALSE(NODE_HAS_SERVO(1)); }
void test_node_has_servo_z_axis(void)    { TEST_ASSERT_FALSE(NODE_HAS_SERVO(2)); }
void test_node_has_servo_a_axis(void)    { TEST_ASSERT_FALSE(NODE_HAS_SERVO(3)); }
void test_node_has_servo_pincher(void)   { TEST_ASSERT_TRUE (NODE_HAS_SERVO(4)); }
void test_node_has_servo_player(void)    { TEST_ASSERT_TRUE (NODE_HAS_SERVO(5)); }

/* =========================================================================
 * NODE_HAS_TOF — all nodes except Player (5)
 * ========================================================================= */

void test_node_has_tof_x_axis(void)     { TEST_ASSERT_TRUE (NODE_HAS_TOF(1)); }
void test_node_has_tof_z_axis(void)     { TEST_ASSERT_TRUE (NODE_HAS_TOF(2)); }
void test_node_has_tof_a_axis(void)     { TEST_ASSERT_TRUE (NODE_HAS_TOF(3)); }
void test_node_has_tof_pincher(void)    { TEST_ASSERT_TRUE (NODE_HAS_TOF(4)); }
void test_node_has_tof_player(void)     { TEST_ASSERT_FALSE(NODE_HAS_TOF(5)); }

/* =========================================================================
 * NODE_HAS_POT — A axis (3) only
 * ========================================================================= */

void test_node_has_pot_x_axis(void)     { TEST_ASSERT_FALSE(NODE_HAS_POT(1)); }
void test_node_has_pot_z_axis(void)     { TEST_ASSERT_FALSE(NODE_HAS_POT(2)); }
void test_node_has_pot_a_axis(void)     { TEST_ASSERT_TRUE (NODE_HAS_POT(3)); }
void test_node_has_pot_pincher(void)    { TEST_ASSERT_FALSE(NODE_HAS_POT(4)); }
void test_node_has_pot_player(void)     { TEST_ASSERT_FALSE(NODE_HAS_POT(5)); }

/* =========================================================================
 * NODE_HAS_LED — A axis (3) and Player (5)
 * ========================================================================= */

void test_node_has_led_x_axis(void)     { TEST_ASSERT_FALSE(NODE_HAS_LED(1)); }
void test_node_has_led_z_axis(void)     { TEST_ASSERT_FALSE(NODE_HAS_LED(2)); }
void test_node_has_led_a_axis(void)     { TEST_ASSERT_TRUE (NODE_HAS_LED(3)); }
void test_node_has_led_pincher(void)    { TEST_ASSERT_FALSE(NODE_HAS_LED(4)); }
void test_node_has_led_player(void)     { TEST_ASSERT_TRUE (NODE_HAS_LED(5)); }

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* NODE_HAS_TMC */
    RUN_TEST(test_node_has_tmc_x_axis);
    RUN_TEST(test_node_has_tmc_z_axis);
    RUN_TEST(test_node_has_tmc_a_axis);
    RUN_TEST(test_node_has_tmc_pincher);
    RUN_TEST(test_node_has_tmc_player);

    /* NODE_HAS_SERVO */
    RUN_TEST(test_node_has_servo_x_axis);
    RUN_TEST(test_node_has_servo_z_axis);
    RUN_TEST(test_node_has_servo_a_axis);
    RUN_TEST(test_node_has_servo_pincher);
    RUN_TEST(test_node_has_servo_player);

    /* NODE_HAS_TOF */
    RUN_TEST(test_node_has_tof_x_axis);
    RUN_TEST(test_node_has_tof_z_axis);
    RUN_TEST(test_node_has_tof_a_axis);
    RUN_TEST(test_node_has_tof_pincher);
    RUN_TEST(test_node_has_tof_player);

    /* NODE_HAS_POT */
    RUN_TEST(test_node_has_pot_x_axis);
    RUN_TEST(test_node_has_pot_z_axis);
    RUN_TEST(test_node_has_pot_a_axis);
    RUN_TEST(test_node_has_pot_pincher);
    RUN_TEST(test_node_has_pot_player);

    /* NODE_HAS_LED */
    RUN_TEST(test_node_has_led_x_axis);
    RUN_TEST(test_node_has_led_z_axis);
    RUN_TEST(test_node_has_led_a_axis);
    RUN_TEST(test_node_has_led_pincher);
    RUN_TEST(test_node_has_led_player);

    return UNITY_END();
}
