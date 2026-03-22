#include "drv_dotstar.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------------- */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t bright;
} dotstar_pixel_t;

static hal_spi_t      *s_spi;
static uint8_t         s_count;
static dotstar_pixel_t s_pixels[DOTSTAR_MAX_LEDS];

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

void drv_dotstar_init(hal_spi_t *spi, uint8_t num_leds)
{
    s_spi   = spi;
    s_count = (num_leds > DOTSTAR_MAX_LEDS) ? DOTSTAR_MAX_LEDS : num_leds;

    for (uint8_t i = 0; i < DOTSTAR_MAX_LEDS; i++) {
        s_pixels[i] = (dotstar_pixel_t){ 0, 0, 0, 0x1Fu };  /* off, full brightness */
    }
}

/* -------------------------------------------------------------------------
 * Pixel buffer
 * ------------------------------------------------------------------------- */

void drv_dotstar_set_pixel(uint8_t idx,
                            uint8_t r, uint8_t g, uint8_t b,
                            uint8_t brightness)
{
    if (idx >= s_count) {
        return;
    }
    s_pixels[idx] = (dotstar_pixel_t){ r, g, b, brightness & 0x1Fu };
}

void drv_dotstar_clear(void)
{
    for (uint8_t i = 0; i < s_count; i++) {
        s_pixels[i].r = 0;
        s_pixels[i].g = 0;
        s_pixels[i].b = 0;
    }
}

/* -------------------------------------------------------------------------
 * Output
 *
 * SPI transfers are 4 bytes each.  The hal_spi interface requires a non-NULL
 * rx buffer; we use a local dummy since DotStar data flows one direction only.
 * ------------------------------------------------------------------------- */

void drv_dotstar_show(void)
{
    uint8_t dummy[4];

    /* Start frame: 4 × 0x00 */
    uint8_t start[4] = {0x00u, 0x00u, 0x00u, 0x00u};
    s_spi->transfer(start, dummy, 4);

    /* Pixel frames: [0xE0 | brightness5, Blue, Green, Red] */
    for (uint8_t i = 0; i < s_count; i++) {
        uint8_t px[4] = {
            0xE0u | s_pixels[i].bright,
            s_pixels[i].b,
            s_pixels[i].g,
            s_pixels[i].r,
        };
        s_spi->transfer(px, dummy, 4);
    }

    /* End frame: 4 × 0xFF (≥32 clock pulses, covers ≤64 LEDs) */
    uint8_t end[4] = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    s_spi->transfer(end, dummy, 4);
}
