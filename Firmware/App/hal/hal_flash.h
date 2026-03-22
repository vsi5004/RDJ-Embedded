#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdint.h>

/* Flash HAL abstraction — double-buffer config storage.
 *
 * The STM32G431 has 128KB flash with the last two 2KB pages reserved for
 * persistent node configuration (see cfg_flash.c).  On target, the STM32
 * HAL erases and programs flash in double-word (8-byte) units.
 *
 * In tests, mock_flash.c backs all three operations with a RAM array so the
 * full double-buffer algorithm runs without touching real hardware.
 */

typedef struct {
    /* Erase one 2KB flash page starting at addr.
     * addr must be the start of a flash page (FLASH_SLOT_A_ADDR or B).
     * Returns 0 on success, non-zero on hardware error. */
    int (*erase)(uint32_t addr);

    /* Write len bytes from data to flash starting at addr.
     * On STM32G4: addr and len must both be multiples of 8 (double-word).
     * Returns 0 on success, non-zero on hardware error. */
    int (*write)(uint32_t addr, const uint8_t *data, uint32_t len);

    /* Read len bytes from flash at addr into data.
     * Flash is memory-mapped on STM32G4; on the mock this copies from RAM. */
    int (*read)(uint32_t addr, uint8_t *data, uint32_t len);
} hal_flash_t;

#endif /* HAL_FLASH_H */
