#include "hal_can.h"
#include "fdcan.h"   /* CubeMX-generated: extern FDCAN_HandleTypeDef hfdcan1 */

static int can_transmit(uint32_t id, uint8_t *data, uint8_t len)
{
    FDCAN_TxHeaderTypeDef hdr = {
        .Identifier          = id,
        .IdType              = FDCAN_STANDARD_ID,
        .TxFrameType         = FDCAN_DATA_FRAME,
        .DataLength          = (uint32_t)len << 16,  /* FDCAN DLC encoding */
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch       = FDCAN_BRS_OFF,
        .FDFormat            = FDCAN_CLASSIC_CAN,
        .TxEventFifoControl  = FDCAN_NO_TX_EVENTS,
        .MessageMarker       = 0,
    };
    HAL_StatusTypeDef s = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &hdr, data);
    return (s == HAL_OK) ? 0 : -1;
}

hal_can_t hal_can1 = { .transmit = can_transmit };
