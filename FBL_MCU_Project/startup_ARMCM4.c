#include <stdint.h>

#define RAM_TOP_STACK   (0x20010000UL)

extern int main(void);

void Reset_Handler(void) {
    main();
    while (1);
}

void Default_Handler(void) {
    while (1);
}

/* Cortex-M Vector Table placed in .isr_vector */
__attribute__((section(".isr_vector"), used))
const uint32_t vector_table[] = {
    RAM_TOP_STACK,              /* Initial Stack Pointer (Top of 64KB SRAM) */
    (uint32_t)&Reset_Handler,   /* Reset Handler */
    (uint32_t)&Default_Handler, /* NMI Handler */
    (uint32_t)&Default_Handler  /* HardFault Handler */
};
