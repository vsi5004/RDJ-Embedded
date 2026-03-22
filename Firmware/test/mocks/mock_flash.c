#include "mock_flash.h"
#include <string.h>

uint8_t mock_flash_mem[MOCK_FLASH_SIZE];
int     mock_flash_erase_count = 0;
int     mock_flash_write_count = 0;

void mock_flash_reset(void)
{
    memset(mock_flash_mem, 0xFFu, sizeof(mock_flash_mem));
    mock_flash_erase_count = 0;
    mock_flash_write_count = 0;
}

static int mock_erase(uint32_t addr)
{
    if (addr < MOCK_FLASH_BASE) return -1;
    uint32_t offset = addr - MOCK_FLASH_BASE;
    if (offset + 0x800u > MOCK_FLASH_SIZE) return -1;
    memset(mock_flash_mem + offset, 0xFFu, 0x800u);
    mock_flash_erase_count++;
    return 0;
}

static int mock_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (addr < MOCK_FLASH_BASE) return -1;
    uint32_t offset = addr - MOCK_FLASH_BASE;
    if (offset + len > MOCK_FLASH_SIZE) return -1;
    memcpy(mock_flash_mem + offset, data, len);
    mock_flash_write_count++;
    return 0;
}

static int mock_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    if (addr < MOCK_FLASH_BASE) return -1;
    uint32_t offset = addr - MOCK_FLASH_BASE;
    if (offset + len > MOCK_FLASH_SIZE) return -1;
    memcpy(data, mock_flash_mem + offset, len);
    return 0;
}

hal_flash_t mock_hal_flash = {
    .erase = mock_erase,
    .write = mock_write,
    .read  = mock_read,
};
