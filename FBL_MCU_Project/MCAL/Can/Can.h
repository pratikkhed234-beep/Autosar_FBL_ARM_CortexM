#ifndef CAN_H
#define CAN_H

#include "Can_GeneralTypes.h"

/* Standard diagnostic CAN IDs */
#define CAN_DIAG_RX_ID      (0x7E0UL) /* Tester to ECU */
#define CAN_DIAG_TX_ID      (0x7E8UL) /* ECU to Tester */

void Can_Init(void);
Can_ReturnType Can_Write(Can_PduType* PduInfo);
void Can_RxIndication(Can_IdType CanId, uint8_t CanDlc, const uint8_t* CanSduPtr);

#endif /* CAN_H */
