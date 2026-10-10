#include "Dcm.h"
#include "MCAL/Fls/Fls.h"
#include "App/BootManager/BootManager.h"
#include <string.h>

#define UDS_ROUTINE_ERASE_FLASH      (0xFF00U)
#define UDS_ROUTINE_CHECK_MEMORY     (0x0202U)

static uint8_t g_current_session = 0x01;
static bool g_security_unlocked  = false;
static uint32_t g_transfer_address = 0;
static uint32_t g_total_transfer_bytes = 0;

void Dcm_Init(void) {
    g_current_session = 0x01;
    g_security_unlocked = false;
    g_transfer_address = 0;
    g_total_transfer_bytes = 0;
}

void Dcm_ProcessDiagnosticRequest(const uint8_t* req, uint16_t req_len, uint8_t* resp, uint16_t* resp_len) {
    if (req_len == 0 || req == NULL || resp == NULL || resp_len == NULL) {
        return;
    }

    uint8_t sid = req[0];

    switch (sid) {
        case 0x10: /* DiagnosticSessionControl */
            g_current_session = req[1];
            resp[0] = 0x50;
            resp[1] = req[1];
            resp[2] = 0x00; resp[3] = 0x32; /* P2 = 50ms */
            resp[4] = 0x01; resp[5] = 0xF4; /* P2* = 5000ms */
            *resp_len = 6;
            break;

        case 0x27: /* SecurityAccess */
            if (req[1] == 0x01) { /* Request Seed */
                resp[0] = 0x67;
                resp[1] = 0x01;
                resp[2] = 0x12; resp[3] = 0x34; resp[4] = 0x56; resp[5] = 0x78;
                *resp_len = 6;
            } else if (req[1] == 0x02) { /* Send Key */
                if (req[2] == 0x9A && req[3] == 0xBC && req[4] == 0xDE && req[5] == 0xF0) {
                    g_security_unlocked = true;
                    resp[0] = 0x67;
                    resp[1] = 0x02;
                    *resp_len = 2;
                } else {
                    resp[0] = 0x7F; resp[1] = 0x27; resp[2] = 0x35; /* Invalid Key */
                    *resp_len = 3;
                }
            }
            break;

        case 0x31: /* RoutineControl */
            if (req[1] == 0x01) { /* StartRoutine */
                uint16_t routine_id = ((uint16_t)req[2] << 8) | req[3];
                if (routine_id == UDS_ROUTINE_ERASE_FLASH) {
                    if (Fls_Erase(FLS_APP_BASE_ADDRESS, (FLS_APP_END_ADDRESS - FLS_APP_BASE_ADDRESS + 1)) == E_OK) {
                        resp[0] = 0x71; resp[1] = 0x01; resp[2] = req[2]; resp[3] = req[3];
                        *resp_len = 4;
                    } else {
                        resp[0] = 0x7F; resp[1] = 0x31; resp[2] = 0x72;
                        *resp_len = 3;
                    }
                } else if (routine_id == UDS_ROUTINE_CHECK_MEMORY) {
                    /* Verify CRC32 sent by host */
                    uint32_t expected_crc = ((uint32_t)req[4] << 24) | ((uint32_t)req[5] << 16) |
                                            ((uint32_t)req[6] << 8)  | (uint32_t)req[7];

                    uint8_t read_back_buf[256];
                    uint32_t verify_len = (g_total_transfer_bytes < sizeof(read_back_buf)) ? g_total_transfer_bytes : sizeof(read_back_buf);
                    Fls_Read(FLS_APP_BASE_ADDRESS, read_back_buf, verify_len);
                    uint32_t actual_crc = BootManager_CalculateCRC32(read_back_buf, verify_len);

                    if (actual_crc == expected_crc) {
                        resp[0] = 0x71; resp[1] = 0x01; resp[2] = req[2]; resp[3] = req[3];
                        resp[4] = 0x00; /* Verification OK */
                        *resp_len = 5;
                    } else {
                        resp[0] = 0x7F; resp[1] = 0x31; resp[2] = 0x72; /* Verification Failed */
                        *resp_len = 3;
                    }
                }
            }
            break;

        case 0x34: /* RequestDownload */
            g_transfer_address = ((uint32_t)req[4] << 24) | ((uint32_t)req[5] << 16) | ((uint32_t)req[6] << 8) | req[7];
            g_total_transfer_bytes = 0;
            resp[0] = 0x74;
            resp[1] = 0x20; /* Length format */
            resp[2] = 0x01; resp[3] = 0x00; /* Max block length = 256 bytes */
            *resp_len = 4;
            break;

        case 0x36: /* TransferData */
            {
                uint16_t chunk_len = req_len - 2;
                Fls_Write(g_transfer_address + g_total_transfer_bytes, &req[2], chunk_len);
                g_total_transfer_bytes += chunk_len;
                resp[0] = 0x76;
                resp[1] = req[1]; /* Block sequence counter */
                *resp_len = 2;
            }
            break;

        case 0x37: /* RequestTransferExit */
            resp[0] = 0x77;
            *resp_len = 1;
            break;

        case 0x11: /* ECUReset */
            resp[0] = 0x51;
            resp[1] = req[1];
            *resp_len = 2;
            break;

        default:
            resp[0] = 0x7F;
            resp[1] = sid;
            resp[2] = 0x11; /* ServiceNotSupported */
            *resp_len = 3;
            break;
    }
}
