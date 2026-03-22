#ifndef MOCK_FLASH_H
#define MOCK_FLASH_H

#include <stdint.h>
#include "hal_flash.h"

/* Simulated flash address range — mirrors the two reserved config pages.
 * Slot A: [MOCK_FLASH_BASE,             MOCK_FLASH_BASE + 0x07FF]
 * Slot B: [MOCK_FLASH_BASE + 0x0800,   MOCK_FLASH_BASE + 0x0FFF] */
#define MOCK_FLASH_BASE  0x0801F000u
#define MOCK_FLASH_SIZE  0x1000u   /* 4KB — both config slots */

/* Direct access to the simulated flash memory.
 * Tests can read this to verify slot contents after cfg_flash_save(),
 * or write into it (using memcpy) to set up corruption scenarios. */
extern uint8_t mock_flash_mem[MOCK_FLASH_SIZE];

/* Counters — reset with mock_flash_reset() */
extern int mock_flash_erase_count;
extern int mock_flash_write_count;

/* Reset: fills mock_flash_mem with 0xFF (erased state) and zeroes counters */
void mock_flash_reset(void);

extern hal_flash_t mock_hal_flash;

#endif /* MOCK_FLASH_H */
