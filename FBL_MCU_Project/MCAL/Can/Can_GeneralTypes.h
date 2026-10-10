#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t Can_IdType;
typedef uint8_t  PduIdType;

typedef struct {
    Can_IdType  id;
    uint8_t     length;
    uint8_t*    sdu;
    PduIdType   swPduHandle;
} Can_PduType;

typedef enum {
    CAN_OK = 0,
    CAN_NOT_OK,
    CAN_BUSY
} Can_ReturnType;

#endif /* CAN_GENERAL_TYPES_H */
