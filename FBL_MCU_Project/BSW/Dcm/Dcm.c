#include "Dcm.h"
#include "../../MCAL/Fls/Fls.h"
#include <string.h>

static Dcm_SessionType current_session = DCM_SESSION_DEFAULT;
static Dcm_SecurityStateType security_state = DCM_SEC_LOCKED;
static uint32_t active_seed = 0x12345678UL;

/* Download tracking */
static uint32_t target_download_addr = 0;
static uint32_t target_download_len = 0;
static uint8_t expected_block_seq = 1;

static void Dcm_SendNegativeResponse(uint8_t sid, uint8_t nrc, uint8_t* resp_buf, uint16_t* resp_len) {
    resp_buf[0] = SID_NEGATIVE_RESPONSE;
    resp_buf[1] = sid;
    resp_buf[2] = nrc;
    *resp_len = 3;
}

void Dcm_Init(void) {
    current_session = DCM_SESSION_DEFAULT;
    security_state = DCM_SEC_LOCKED;
    expected_block_seq = 1;
}

void Dcm_ProcessDiagnosticRequest(const uint8_t* req_buf, uint16_t req_len, uint8_t* resp_buf, uint16_t* resp_len) {
    if (req_len == 0) return;
    uint8_t sid = req_buf[0];

    switch (sid) {
        /* --- 0x10 Diagnostic Session Control --- */
        case SID_DIAGNOSTIC_SESSION_CONTROL:
            if (req_len < 2) {
                Dcm_SendNegativeResponse(sid, NRC_INCORRECT_MESSAGE_LENGTH, resp_buf, resp_len);
                return;
            }
            if (req_buf[1] == DCM_SESSION_DEFAULT || req_buf[1] == DCM_SESSION_EXTENDED || req_buf[1] == DCM_SESSION_PROGRAMMING) {
                current_session = (Dcm_SessionType)req_buf[1];
                resp_buf[0] = sid + 0x40;
                resp_buf[1] = req_buf[1];
                /* P2 and P2* timing parameters */
                resp_buf[2] = 0x00; resp_buf[3] = 0x32; /* P2 = 50 ms */
                resp_buf[4] = 0x01; resp_buf[5] = 0xF4; /* P2* = 5000 ms */
                *resp_len = 6;
            } else {
                Dcm_SendNegativeResponse(sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, resp_buf, resp_len);
            }
            break;

        /* --- 0x27 Security Access --- */
        case SID_SECURITY_ACCESS:
            if (req_buf[1] == 0x01) { /* Request Seed */
                active_seed = 0xAABBCCDDUL; /* Example fixed seed / HW TRNG */
                resp_buf[0] = sid + 0x40;
                resp_buf[1] = 0x01;
                resp_buf[2] = (uint8_t)(active_seed >> 24);
                resp_buf[3] = (uint8_t)(active_seed >> 16);
                resp_buf[4] = (uint8_t)(active_seed >> 8);
                resp_buf[5] = (uint8_t)(active_seed);
                *resp_len = 6;
                security_state = DCM_SEC_SEED_SENT;
            } else if (req_buf[1] == 0x02) { /* Send Key */
                if (security_state != DCM_SEC_SEED_SENT || req_len < 6) {
                    Dcm_SendNegativeResponse(sid, NRC_REQUEST_SEQUENCE_ERROR, resp_buf, resp_len);
                    return;
                }
                /* Verify XOR key match: key == seed ^ 0x5A */
                uint32_t expected_key = active_seed ^ 0x5A5A5A5AUL;
                uint32_t received_key = ((uint32_t)req_buf[2] << 24) | ((uint32_t)req_buf[3] << 16) |
                                        ((uint32_t)req_buf[4] << 8)  | ((uint32_t)req_buf[5]);
                if (received_key == expected_key) {
                    security_state = DCM_SEC_UNLOCKED;
                    resp_buf[0] = sid + 0x40;
                    resp_buf[1] = 0x02;
                    *resp_len = 2;
                } else {
                    security_state = DCM_SEC_LOCKED;
                    Dcm_SendNegativeResponse(sid, NRC_INVALID_KEY, resp_buf, resp_len);
                }
            }
            break;

        /* --- 0x31 Routine Control (Erase Flash) --- */
        case SID_ROUTINE_CONTROL:
            if (security_state != DCM_SEC_UNLOCKED) {
                Dcm_SendNegativeResponse(sid, NRC_SECURITY_ACCESS_DENIED, resp_buf, resp_len);
                return;
            }
            /* Sub-function 0x01 (Start Routine), Routine ID 0xFF00 (Erase Flash) */
            if (req_buf[1] == 0x01 && req_buf[2] == 0xFF && req_buf[3] == 0x00) {
                if (Fls_Erase(FLS_APP_BASE_ADDRESS, (FLS_APP_END_ADDRESS - FLS_APP_BASE_ADDRESS + 1)) == FLS_OK) {
                    resp_buf[0] = sid + 0x40;
                    resp_buf[1] = 0x01;
                    resp_buf[2] = 0xFF;
                    resp_buf[3] = 0x00;
                    *resp_len = 4;
                } else {
                    Dcm_SendNegativeResponse(sid, NRC_CONDITIONS_NOT_CORRECT, resp_buf, resp_len);
                }
            }
            break;

        /* --- 0x34 Request Download --- */
        case SID_REQUEST_DOWNLOAD:
            if (security_state != DCM_SEC_UNLOCKED) {
                Dcm_SendNegativeResponse(sid, NRC_SECURITY_ACCESS_DENIED, resp_buf, resp_len);
                return;
            }
            /* Unpack Target Address & Size (assuming 4-byte address, 4-byte size) */
            target_download_addr = ((uint32_t)req_buf[3] << 24) | ((uint32_t)req_buf[4] << 16) |
                                   ((uint32_t)req_buf[5] << 8)  | ((uint32_t)req_buf[6]);
            target_download_len  = ((uint32_t)req_buf[7] << 24) | ((uint32_t)req_buf[8] << 16) |
                                   ((uint32_t)req_buf[9] << 8)  | ((uint32_t)req_buf[10]);
            expected_block_seq = 1;

            resp_buf[0] = sid + 0x40;
            resp_buf[1] = 0x20; /* Length format identifier */
            resp_buf[2] = 0x01; resp_buf[3] = 0x00; /* Max block length = 256 bytes */
            *resp_len = 4;
            break;

        /* --- 0x36 Transfer Data --- */
        case SID_TRANSFER_DATA:
            if (req_buf[1] != expected_block_seq) {
                Dcm_SendNegativeResponse(sid, NRC_REQUEST_SEQUENCE_ERROR, resp_buf, resp_len);
                return;
            }
            uint16_t data_chunk_len = req_len - 2;
            if (Fls_Write(target_download_addr, &req_buf[2], data_chunk_len) == FLS_OK) {
                target_download_addr += data_chunk_len;
                expected_block_seq++;
                resp_buf[0] = sid + 0x40;
                resp_buf[1] = req_buf[1];
                *resp_len = 2;
            } else {
                Dcm_SendNegativeResponse(sid, NRC_CONDITIONS_NOT_CORRECT, resp_buf, resp_len);
            }
            break;

        /* --- 0x37 Request Transfer Exit --- */
        case SID_REQUEST_TRANSFER_EXIT:
            resp_buf[0] = sid + 0x40;
            *resp_len = 1;
            break;

        /* --- 0x11 ECU Reset --- */
        case SID_ECU_RESET:
            resp_buf[0] = sid + 0x40;
            resp_buf[1] = req_buf[1];
            *resp_len = 2;
            /* In a real target, trigger NVIC_SystemReset() after sending this response */
            break;

        default:
            Dcm_SendNegativeResponse(sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, resp_buf, resp_len);
            break;
    }
}
