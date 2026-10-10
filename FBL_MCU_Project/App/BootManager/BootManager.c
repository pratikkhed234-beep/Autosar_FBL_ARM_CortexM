#include "BootManager.h"

#define APP_START_ADDRESS      (0x08008800UL)
#define APP_VECTOR_TABLE       ((uint32_t*)APP_START_ADDRESS)

typedef void (*pFunction)(void);

bool BootManager_CheckAppIntegrity(void) {
    uint32_t app_msp = APP_VECTOR_TABLE[0];
    
    /* Cortex-M SRAM range check (0x20000000 - 0x20010000 for 64KB) */
    if ((app_msp >= 0x20000000UL) && (app_msp <= 0x20010000UL)) {
        return true;
    }
    return false;
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
