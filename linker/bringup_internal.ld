ENTRY(Reset_Handler)

MEMORY
{
  /* Standalone diagnostic temporarily occupies the bootloader slot. */
  FLASH (rx)       : ORIGIN = 0x08000000, LENGTH = 128K
  DTCM (rw)        : ORIGIN = 0x20000000, LENGTH = 128K
  AXI_SRAM (rwx)   : ORIGIN = 0x24000000, LENGTH = 512K
  DMA_D2 (rw)      : ORIGIN = 0x30000000, LENGTH = 32K
  SRAM_D2 (rw)     : ORIGIN = 0x30008000, LENGTH = 256K
  SRAM_D3 (rw)     : ORIGIN = 0x38000000, LENGTH = 64K
}

REGION_ALIAS("ROM", FLASH)
REGION_ALIAS("RAM", AXI_SRAM)
INCLUDE linker/common_internal_sections.ld
