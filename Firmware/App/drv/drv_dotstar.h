#ifndef DRV_DOTSTAR_H
#define DRV_DOTSTAR_H

#include <stdint.h>
#include "hal_spi.h"

/* APA102 / DotStar LED driver (SPI2 — unidirectional, no CS).
 *
 * Frame format:
 *   Start  : 4 × 0x00
 *   Per LED: [0xE0 | brightness5, Blue, Green, Red]  (4 bytes)
 *   End    : 4 × 0xFF  (≥32 clock pulses, sufficient for ≤64 LEDs)
 *
 * drv_dotstar_show() issues one SPI transfer per section (start, each pixel,
 * end) so each call is ≤4 bytes and can reuse the same SPI mock as the rest
 * of the test suite.
 *
 * brightness5 is a 5-bit value (0–31); values above 31 are masked to 5 bits.
 * Default brightness is 31 (maximum) unless set explicitly.
 *
 * Call init() once, then set_pixel() for any LEDs you want to change, then
 * show() to push the whole buffer out to the chain.
 */

#define DOTSTAR_MAX_LEDS  60u

/* Bind the SPI HAL instance and configure the strip length.
 * All pixels initialise to off (RGB = 0, brightness = 31). */
void drv_dotstar_init(hal_spi_t *spi, uint8_t num_leds);

/* Update one pixel in the internal buffer.
 * idx        : 0-based LED index; silently ignored if ≥ num_leds.
 * r, g, b    : 8-bit colour components.
 * brightness : 0–31 (values above 31 are masked to 5 bits). */
void drv_dotstar_set_pixel(uint8_t idx,
                            uint8_t r, uint8_t g, uint8_t b,
                            uint8_t brightness);

/* Push the internal pixel buffer to the LED chain via SPI. */
void drv_dotstar_show(void);

/* Set all pixels in the buffer to off (RGB = 0). Does not call show(). */
void drv_dotstar_clear(void);

#endif /* DRV_DOTSTAR_H */
