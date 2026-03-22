/* Phase 5 — APA102 / DotStar LED driver tests
 *
 * Tests cover frame format:
 *   - Start frame : 4 × 0x00 (one 4-byte SPI transfer)
 *   - Pixel frames: [0xE0 | brightness5, Blue, Green, Red] per LED
 *       - Colour byte order on the wire is B, G, R (not R, G, B)
 *       - Brightness is masked to 5 bits; header byte = 0xE0 | (brightness & 0x1F)
 *       - Full brightness (31) → header byte 0xFF
 *   - End frame  : 4 × 0xFF (one 4-byte SPI transfer)
 *   - Total SPI transfers = 2 + num_pixels for a full show()
 *
 * The driver issues one SPI transfer per frame section (start, each pixel,
 * end), each 4 bytes, so every call fits within the 8-byte mock SPI record.
 *
 * Additional tests:
 *   - set_pixel() with idx ≥ num_leds is silently ignored
 *   - clear() zeroes all pixel RGB values (does not call show())
 *   - Default pixel state after init is off (black, max brightness)
 *
 * Naming convention: test_dotstar_<scenario>
 */

#include "unity.h"
#include "drv_dotstar.h"
#include "mock_spi.h"

#define NUM_LEDS  3u

void setUp(void)
{
    mock_spi_reset();
    drv_dotstar_init(&mock_hal_spi, NUM_LEDS);
}

void tearDown(void) {}

/* =========================================================================
 * Helpers
 * ========================================================================= */

/* Index of the SPI transfer for pixel i in a show() call:
 *   0       = start frame
 *   1..n    = pixel frames
 *   n+1     = end frame
 */
static int pixel_xfer(uint8_t i) { return 1 + i; }
static int end_xfer(void)        { return 1 + NUM_LEDS; }

/* =========================================================================
 * Group 1: start frame
 * ========================================================================= */

void test_dotstar_show_sends_start_frame_first(void)
{
    drv_dotstar_show();

    TEST_ASSERT_EQUAL_UINT8(0x00, mock_spi_log[0].tx[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, mock_spi_log[0].tx[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, mock_spi_log[0].tx[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, mock_spi_log[0].tx[3]);
}

void test_dotstar_start_frame_is_4_bytes(void)
{
    drv_dotstar_show();

    TEST_ASSERT_EQUAL_UINT16(4, mock_spi_log[0].len);
}

/* =========================================================================
 * Group 2: pixel frame format
 * ========================================================================= */

void test_dotstar_pixel_header_byte_at_full_brightness(void)
{
    drv_dotstar_set_pixel(0, 0xFF, 0xFF, 0xFF, 31);
    drv_dotstar_show();

    /* brightness=31 → header = 0xE0 | 0x1F = 0xFF */
    TEST_ASSERT_EQUAL_HEX8(0xFF, mock_spi_log[pixel_xfer(0)].tx[0]);
}

void test_dotstar_pixel_header_byte_at_half_brightness(void)
{
    drv_dotstar_set_pixel(0, 0, 0, 0, 15);
    drv_dotstar_show();

    /* brightness=15 → header = 0xE0 | 0x0F = 0xEF */
    TEST_ASSERT_EQUAL_HEX8(0xEF, mock_spi_log[pixel_xfer(0)].tx[0]);
}

void test_dotstar_pixel_brightness_masked_to_5_bits(void)
{
    drv_dotstar_set_pixel(0, 0, 0, 0, 63);  /* 63 & 0x1F = 31 */
    drv_dotstar_show();

    /* brightness 63 masked to 31 → header 0xFF */
    TEST_ASSERT_EQUAL_HEX8(0xFF, mock_spi_log[pixel_xfer(0)].tx[0]);
}

void test_dotstar_pixel_colour_order_is_bgr(void)
{
    drv_dotstar_set_pixel(0, 0x11, 0x22, 0x33, 31);  /* r=0x11, g=0x22, b=0x33 */
    drv_dotstar_show();

    /* Wire order after header: Blue, Green, Red */
    TEST_ASSERT_EQUAL_HEX8(0x33, mock_spi_log[pixel_xfer(0)].tx[1]);  /* B */
    TEST_ASSERT_EQUAL_HEX8(0x22, mock_spi_log[pixel_xfer(0)].tx[2]);  /* G */
    TEST_ASSERT_EQUAL_HEX8(0x11, mock_spi_log[pixel_xfer(0)].tx[3]);  /* R */
}

void test_dotstar_pixel_frame_is_4_bytes(void)
{
    drv_dotstar_show();

    TEST_ASSERT_EQUAL_UINT16(4, mock_spi_log[pixel_xfer(0)].len);
}

void test_dotstar_second_pixel_is_independent(void)
{
    drv_dotstar_set_pixel(0, 0xAA, 0xBB, 0xCC, 31);
    drv_dotstar_set_pixel(1, 0x01, 0x02, 0x03, 10);
    drv_dotstar_show();

    /* pixel 1 header: 0xE0 | 10 = 0xEA */
    TEST_ASSERT_EQUAL_HEX8(0xEA, mock_spi_log[pixel_xfer(1)].tx[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03, mock_spi_log[pixel_xfer(1)].tx[1]);  /* B */
    TEST_ASSERT_EQUAL_HEX8(0x02, mock_spi_log[pixel_xfer(1)].tx[2]);  /* G */
    TEST_ASSERT_EQUAL_HEX8(0x01, mock_spi_log[pixel_xfer(1)].tx[3]);  /* R */
}

/* =========================================================================
 * Group 3: end frame
 * ========================================================================= */

void test_dotstar_show_sends_end_frame_last(void)
{
    drv_dotstar_show();

    int last = end_xfer();
    TEST_ASSERT_EQUAL_HEX8(0xFF, mock_spi_log[last].tx[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, mock_spi_log[last].tx[1]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, mock_spi_log[last].tx[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, mock_spi_log[last].tx[3]);
}

void test_dotstar_end_frame_is_4_bytes(void)
{
    drv_dotstar_show();

    TEST_ASSERT_EQUAL_UINT16(4, mock_spi_log[end_xfer()].len);
}

/* =========================================================================
 * Group 4: transfer count and bounds checking
 * ========================================================================= */

void test_dotstar_show_total_spi_transfer_count(void)
{
    drv_dotstar_show();

    /* 1 start + NUM_LEDS pixels + 1 end */
    TEST_ASSERT_EQUAL_INT(2 + NUM_LEDS, mock_spi_call_count);
}

void test_dotstar_set_pixel_out_of_range_is_ignored(void)
{
    drv_dotstar_set_pixel(NUM_LEDS, 0xFF, 0xFF, 0xFF, 31);  /* idx == num_leds */
    drv_dotstar_show();

    /* Transfer count unchanged — no extra pixel in the frame */
    TEST_ASSERT_EQUAL_INT(2 + NUM_LEDS, mock_spi_call_count);
}

/* =========================================================================
 * Group 5: clear() and default state
 * ========================================================================= */

void test_dotstar_default_pixel_is_black(void)
{
    /* After init, no set_pixel() call — pixels should output black (0,0,0) */
    drv_dotstar_show();

    /* pixel 0: B=0, G=0, R=0 */
    TEST_ASSERT_EQUAL_HEX8(0x00, mock_spi_log[pixel_xfer(0)].tx[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, mock_spi_log[pixel_xfer(0)].tx[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, mock_spi_log[pixel_xfer(0)].tx[3]);
}

void test_dotstar_clear_zeroes_pixel_rgb(void)
{
    drv_dotstar_set_pixel(0, 0xFF, 0xFF, 0xFF, 31);
    drv_dotstar_clear();
    drv_dotstar_show();

    TEST_ASSERT_EQUAL_HEX8(0x00, mock_spi_log[pixel_xfer(0)].tx[1]);  /* B */
    TEST_ASSERT_EQUAL_HEX8(0x00, mock_spi_log[pixel_xfer(0)].tx[2]);  /* G */
    TEST_ASSERT_EQUAL_HEX8(0x00, mock_spi_log[pixel_xfer(0)].tx[3]);  /* R */
}

void test_dotstar_clear_does_not_call_show(void)
{
    drv_dotstar_clear();

    TEST_ASSERT_EQUAL_INT(0, mock_spi_call_count);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* start frame */
    RUN_TEST(test_dotstar_show_sends_start_frame_first);
    RUN_TEST(test_dotstar_start_frame_is_4_bytes);

    /* pixel frames */
    RUN_TEST(test_dotstar_pixel_header_byte_at_full_brightness);
    RUN_TEST(test_dotstar_pixel_header_byte_at_half_brightness);
    RUN_TEST(test_dotstar_pixel_brightness_masked_to_5_bits);
    RUN_TEST(test_dotstar_pixel_colour_order_is_bgr);
    RUN_TEST(test_dotstar_pixel_frame_is_4_bytes);
    RUN_TEST(test_dotstar_second_pixel_is_independent);

    /* end frame */
    RUN_TEST(test_dotstar_show_sends_end_frame_last);
    RUN_TEST(test_dotstar_end_frame_is_4_bytes);

    /* transfer count and bounds */
    RUN_TEST(test_dotstar_show_total_spi_transfer_count);
    RUN_TEST(test_dotstar_set_pixel_out_of_range_is_ignored);

    /* clear and defaults */
    RUN_TEST(test_dotstar_default_pixel_is_black);
    RUN_TEST(test_dotstar_clear_zeroes_pixel_rgb);
    RUN_TEST(test_dotstar_clear_does_not_call_show);

    return UNITY_END();
}
