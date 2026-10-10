#ifndef DCM_H
#define DCM_H

#include <stdint.h>
#include <stdbool.h>

/* ISO 14229 Service Identifiers (SID) */
#define SID_DIAGNOSTIC_SESSION_CONTROL    (0x10U)
#define SID_ECU_RESET                     (0x11U)
#define SID_SECURITY_ACCESS               (0x27U)
#define SID_ROUTINE_CONTROL               (0x31U)
#define SID_REQUEST_DOWNLOAD              (0x34U)
#define SID_TRANSFER_DATA                 (0x36U)
#define SID_REQUEST_TRANSFER_EXIT         (0x37U)
#define SID_NEGATIVE_RESPONSE             (0x7FU)

/* UDS Negative Response Codes (NRC) */
#define NRC_SUB_FUNCTION_NOT_SUPPORTED    (0x12U)
#define NRC_INCORRECT_MESSAGE_LENGTH      (0x13U)
#define NRC_CONDITIONS_NOT_CORRECT        (0x22U)
#define NRC_REQUEST_SEQUENCE_ERROR        (0x24U)
#define NRC_REQUEST_OUT_OF_RANGE          (0x31U)
#define NRC_SECURITY_ACCESS_DENIED        (0x33U)
#define NRC_INVALID_KEY                   (0x35U)

/* Diagnostic Sessions */
typedef enum {
    DCM_SESSION_DEFAULT = 0x01,
    DCM_SESSION_PROGRAMMING = 0x02,
    DCM_SESSION_EXTENDED = 0x03
} Dcm_SessionType;

/* Security States */
typedef enum {
    DCM_SEC_LOCKED = 0,
    DCM_SEC_SEED_SENT,
    DCM_SEC_UNLOCKED
} Dcm_SecurityStateType;

void Dcm_Init(void);
void Dcm_MainFunction(void);
void Dcm_ProcessDiagnosticRequest(const uint8_t* req_buf, uint16_t req_len, uint8_t* resp_buf, uint16_t* resp_len);

#endif /* DCM_H */
