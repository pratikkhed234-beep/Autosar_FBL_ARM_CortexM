#include "BootManager.h"

#define APP_START_ADDRESS      (0x08008800UL)
#define APP_VECTOR_TABLE       ((uint32_t*)APP_START_ADDRESS)

typedef void (*pFunction)(void);

uint32_t BootManager_CalculateCRC32(const uint8_t *data, uint32_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc = (crc >> 1);
            }
        }
    }
    return ~crc;
}

bool BootManager_CheckAppIntegrity(void) {
    const BootMetaHeader_t *meta = (const BootMetaHeader_t *)BOOT_META_ADDRESS;
    uint32_t app_msp = APP_VECTOR_TABLE[0];

    /* 1. Stack pointer sanity check inside Cortex-M SRAM range */
    if ((app_msp < 0x20000000UL) || (app_msp > 0x20010000UL)) {
        return false;
    }

    /* 2. Metadata pattern check */
    if (meta->magic != APP_VALID_MAGIC) {
        return false;
    }

    /* 3. CRC32 verification over active application memory */
    if (meta->app_size == 0 || meta->app_size > (512 * 1024)) {
        return false;
    }

    uint32_t computed_crc = BootManager_CalculateCRC32((const uint8_t *)APP_START_ADDRESS, meta->app_size);
    return (computed_crc == meta->crc32);
}

void BootManager_JumpToApplication(void) {
    uint32_t app_stack_pointer = APP_VECTOR_TABLE[0];
    uint32_t app_reset_handler = APP_VECTOR_TABLE[1];
    pFunction app_entry = (pFunction)app_reset_handler;

    /* 1. Disable all interrupts */
    __asm__ volatile("cpsid i");

    /* 2. Relocate VTOR to application vector table */
    volatile uint32_t* vtor = (volatile uint32_t*)0xE000ED08UL;
    *vtor = APP_START_ADDRESS;

    /* 3. Set Main Stack Pointer (MSP) */
    __asm__ volatile("msr msp, %0" : : "r"(app_stack_pointer) : );

    /* 4. Branch to application entry point */
    app_entry();
}
