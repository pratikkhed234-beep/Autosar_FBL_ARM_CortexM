/* AUTOSAR Bootloader Flash Memory Partition (e.g., 512KB Flash, 64KB RAM) */

MEMORY
{
  /* 1. Bootloader Space: Vector Table + BSW + Minimal DCM */
  FLASH_BOOT (rx)    : ORIGIN = 0x08000000, LENGTH = 32K

  /* 2. Metadata / Boot Info: Validity Flag, CRC32, Image Version */
  FLASH_META (r)     : ORIGIN = 0x08008000, LENGTH = 2K

  /* 3. Application Space: Target address for UDS RequestDownload (0x34) */
  FLASH_APP  (rx)    : ORIGIN = 0x08008800, LENGTH = 478K

  /* RAM layout */
  RAM        (xrw)   : ORIGIN = 0x20000000, LENGTH = 64K
}

/* Sections definition */
SECTIONS
{
  /* Interrupt Vector Table for Bootloader */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH_BOOT

  /* Bootloader Program Code */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH_BOOT

  /* RAM-critical functions: Fls Driver must run from RAM to avoid CPU bus stalls */
  .ram_functions :
  {
    . = ALIGN(4);
    _sram_func = .;
    *(.ramfunctions)
    *(.ramfunctions*)
    . = ALIGN(4);
    _eram_func = .;
  } >RAM AT> FLASH_BOOT

  /* Initialized data */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH_BOOT

  /* Uninitialized data (BSS) */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >RAM
}
