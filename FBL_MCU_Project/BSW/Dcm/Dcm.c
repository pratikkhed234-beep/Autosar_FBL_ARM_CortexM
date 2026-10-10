#include "Dcm.h"
#include "MCAL/Fls/Fls.h"
#include "App/BootManager/BootManager.h"
#include <string.h>

#define UDS_ROUTINE_ERASE_FLASH      (0xFF00U)
#define UDS_ROUTINE_CHECK_MEMORY     (0x0202U)
#define SECURITY_KEY_MASK            (0x4A92B351UL)

static uint8_t g_current_session = 0x01;
static bool g_security_unlocked  = false;
static uint32_t g_transfer_address = 0;
static uint32_t g_total_transfer_bytes = 0;
static uint32_t g_current_seed = 0x55AA1234UL;

/* Simple dynamic PRNG generator for embedded seed generation */
static uint32_t Dcm_GenerateSeed(void) {
    g_current_seed = (g_current_seed * 1103515245UL + 12345UL) & 0x7FFFFFFFUL;
    if (g_current_seed == 0) {
        g_current_seed = 0x1A2B3C4DUL;
    }
    return g_current_seed;
}

/* Automotive Seed-to-Key transformation function */
static uint32_t Dcm_CalculateKeyFromSeed(uint32_t seed) {
    uint32_t key = seed ^ SECURITY_KEY_MASK;
    key = (key << 5) | (key >> 27); /* Rotate left 5 bits */
    key ^= 0xA5A5A5A5UL;
    return key;
}

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
                uint32_t dynamic_seed = Dcm_GenerateSeed();
                resp[0] = 0x67;
                resp[1] = 0x01;
                resp[2] = (uint8_t)(dynamic_seed >> 24);
                resp[3] = (uint8_t)(dynamic_seed >> 16);
                resp[4] = (uint8_t)(dynamic_seed >> 8);
                resp[5] = (uint8_t)(dynamic_seed);
                *resp_len = 6;
            } else if (req[1] == 0x02) { /* Send Key */
                uint32_t received_key = ((uint32_t)req[2] << 24) | ((uint32_t)req[3] << 16) |
                                        ((uint32_t)req[4] << 8)  | (uint32_t)req[5];
                uint32_t expected_key = Dcm_CalculateKeyFromSeed(g_current_seed);

                if (received_key == expected_key) {
                    g_security_unlocked = true;
                    resp[0] = 0x67;
                    resp[1] = 0x02;
                    *resp_len = 2;
                } else {
                    resp[0] = 0x7F; resp[1] = 0x27; resp[2] = 0x35; /* Invalid Key NRC 0x35 */
                    *resp_len = 3;
                }
            }
            break;

        case 0x31: /* RoutineControl */
            if (req[1] == 0x01) { /* StartRoutine */
                uint16_t routine_id = ((uint16_t)req[2] << 8) | req[3];
                if (routine_id == UDS_ROUTINE_ERASE_FLASH) {
                    if (!g_security_unlocked) {
                        resp[0] = 0x7F; resp[1] = 0x31; resp[2] = 0x33; /* SecurityAccessDenied */
                        *resp_len = 3;
                        return;
                    }
                    if (Fls_Erase(FLS_APP_BASE_ADDRESS, (FLS_APP_END_ADDRESS - FLS_APP_BASE_ADDRESS + 1)) == E_OK) {
                        resp[0] = 0x71; resp[1] = 0x01; resp[2] = req[2]; resp[3] = req[3];
                        *resp_len = 4;
                    } else {
                        resp[0] = 0x7F; resp[1] = 0x31; resp[2] = 0x72;
                        *resp_len = 3;
                    }
                } else if (routine_id == UDS_ROUTINE_CHECK_MEMORY) {
                    uint32_t expected_crc = ((uint32_t)req[4] << 24) | ((uint32_t)req[5] << 16) |
                                            ((uint32_t)req[6] << 8)  | (uint32_t)req[7];

                    uint8_t read_back_buf[256];
                    uint32_t verify_len = (g_total_transfer_bytes < sizeof(read_back_buf)) ? g_total_transfer_bytes : sizeof(read_back_buf);
                    Fls_Read(FLS_APP_BASE_ADDRESS, read_back_buf, verify_len);
                    uint32_t actual_crc = BootManager_CalculateCRC32(read_back_buf, verify_len);

                    if (actual_crc == expected_crc) {
                        resp[0] = 0x71; resp[1] = 0x01; resp[2] = req[2]; resp[3] = req[3];
                        resp[4] = 0x00;
                        *resp_len = 5;
                    } else {
                        resp[0] = 0x7F; resp[1] = 0x31; resp[2] = 0x72;
                        *resp_len = 3;
                    }
                }
            }
            break;

        case 0x34: /* RequestDownload */
            if (!g_security_unlocked) {
                resp[0] = 0x7F; resp[1] = 0x34; resp[2] = 0x33; /* SecurityAccessDenied */
                *resp_len = 3;
                return;
            }
            g_transfer_address = ((uint32_t)req[4] << 24) | ((uint32_t)req[5] << 16) | ((uint32_t)req[6] << 8) | req[7];
            g_total_transfer_bytes = 0;
            resp[0] = 0x74;
            resp[1] = 0x20;
            resp[2] = 0x01; resp[3] = 0x00;
            *resp_len = 4;
            break;

        case 0x36: /* TransferData */
            {
                uint16_t chunk_len = req_len - 2;
                Fls_Write(g_transfer_address + g_total_transfer_bytes, &req[2], chunk_len);
                g_total_transfer_bytes += chunk_len;
                resp[0] = 0x76;
                resp[1] = req[1];
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
            resp[2] = 0x11;
            *resp_len = 3;
            break;
    }
}
