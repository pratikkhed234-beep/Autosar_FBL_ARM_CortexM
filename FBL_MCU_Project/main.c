#include <stdint.h>
#include <stdbool.h>

#include "MCAL/Can/Can.h"
#include "MCAL/Fls/Fls.h"
#include "BSW/Dcm/Dcm.h"
#include "App/BootManager/BootManager.h"

/* Diagnostic reception buffer definitions */
#define DIAG_BUFFER_SIZE    (512U)
static uint8_t diag_rx_buffer[DIAG_BUFFER_SIZE];
static uint16_t diag_rx_len = 0;
static bool diag_request_pending = false;

static uint8_t diag_tx_buffer[DIAG_BUFFER_SIZE];
static uint16_t diag_tx_len = 0;

/* Hook called by CanTp when a full UDS message is reassembled */
void CanTp_RxIndication(uint32_t can_id, const uint8_t* data, uint8_t len) {
    if (len <= DIAG_BUFFER_SIZE) {
        for (uint16_t i = 0; i < len; i++) {
            diag_rx_buffer[i] = data[i];
        }
        diag_rx_len = len;
        diag_request_pending = true;
    }
}

static void SendDiagnosticResponse(const uint8_t* resp, uint16_t len) {
    Can_PduType txPdu;
    txPdu.id = CAN_DIAG_TX_ID;
    txPdu.length = (uint8_t)(len > 8 ? 8 : len);
    txPdu.sdu = (uint8_t*)resp;
    txPdu.swPduHandle = 0;

    Can_Write(&txPdu);
}

int main(void) {
    /* 1. Initialize Microcontroller Hardware & MCAL Drivers */
    Can_Init();
    Fls_Init();
    Dcm_Init();

    /* 2. Boot Timeout Window (e.g., 200 iterations / ~200 ms) */
    uint32_t boot_timeout = 200000UL;
    bool stay_in_bootloader = false;

    while (1) {
        /* Process incoming diagnostic requests */
        if (diag_request_pending) {
            diag_request_pending = false;

            /* If any valid diagnostic request arrives, hold execution in Bootloader */
            stay_in_bootloader = true;

            /* Execute DCM service dispatcher */
            Dcm_ProcessDiagnosticRequest(diag_rx_buffer, diag_rx_len, diag_tx_buffer, &diag_tx_len);

            /* Send response over CAN */
            if (diag_tx_len > 0) {
                SendDiagnosticResponse(diag_tx_buffer, diag_tx_len);
            }
        }

        /* Check if we should branch to the application */
        if (!stay_in_bootloader) {
            if (boot_timeout > 0) {
                boot_timeout--;
            } else {
                /* Timeout expired without diagnostics: attempt application jump */
                if (BootManager_CheckAppIntegrity()) {
                    BootManager_JumpToApplication();
                } else {
                    /* Corrupted or empty app: stay trapped safely in bootloader */
                    stay_in_bootloader = true;
                }
            }
        }
    }

    return 0;
}
