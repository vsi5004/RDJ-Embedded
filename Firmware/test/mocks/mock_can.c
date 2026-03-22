#include "mock_can.h"
#include <string.h>

mock_can_frame_t mock_can_tx_log[MOCK_CAN_MAX_FRAMES];
int              mock_can_tx_count = 0;

void mock_can_reset(void)
{
    memset(mock_can_tx_log, 0, sizeof(mock_can_tx_log));
    mock_can_tx_count = 0;
}

static int mock_transmit(uint32_t id, uint8_t *data, uint8_t len)
{
    if (mock_can_tx_count >= MOCK_CAN_MAX_FRAMES) return -1;
    int idx = mock_can_tx_count++;
    mock_can_tx_log[idx].id  = id;
    mock_can_tx_log[idx].len = len;
    uint8_t copy = (len > 8) ? 8 : len;
    memcpy(mock_can_tx_log[idx].data, data, copy);
    return 0;
}

hal_can_t mock_hal_can = { .transmit = mock_transmit };
