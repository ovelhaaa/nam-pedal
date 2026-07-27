ENTRY(Reset_Handler)

MEMORY
{
  QSPI (rx)        : ORIGIN = 0x90010000, LENGTH = 0x007F0000
  DTCM (rw)        : ORIGIN = 0x20000000, LENGTH = 128K
  AXI_SRAM (rwx)   : ORIGIN = 0x24000000, LENGTH = 512K
  DMA_D2 (rw)      : ORIGIN = 0x30000000, LENGTH = 32K
  SRAM_D2 (rw)     : ORIGIN = 0x30008000, LENGTH = 256K
  SRAM_D3 (rw)     : ORIGIN = 0x38000000, LENGTH = 64K
}

_estack = ORIGIN(AXI_SRAM) + LENGTH(AXI_SRAM);
_Min_Heap_Size = 0x000;
_Min_Stack_Size = 0x1000;

SECTIONS
{
  .qspi_vector :
  {
    . = ALIGN(512);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > QSPI

  .qspi_text :
  {
    . = ALIGN(4);
    *(.text .text.*)
    *(.glue_7) *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } > QSPI

  .qspi_rodata :
  {
    . = ALIGN(4);
    *(.rodata .rodata.*)
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } > QSPI

  .qspi_init_arrays :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
    . = ALIGN(4);
  } > QSPI

  .model_weights_dtcm (NOLOAD) :
  {
    . = ALIGN(32);
    __model_weights_dtcm_start__ = .;
    KEEP(*(.model_weights_dtcm .model_weights_dtcm.*))
    . = ALIGN(32);
    __model_weights_dtcm_end__ = .;
  } > DTCM

  .nam_ring_buffers (NOLOAD) :
  {
    . = ALIGN(32);
    __nam_ring_buffers_start__ = .;
    KEEP(*(.nam_ring_buffers .nam_ring_buffers.*))
    . = ALIGN(32);
    __nam_ring_buffers_end__ = .;
  } > AXI_SRAM

  .audio_work_buffers (NOLOAD) :
  {
    . = ALIGN(32);
    __audio_work_buffers_start__ = .;
    KEEP(*(.audio_work_buffers .audio_work_buffers.*))
    . = ALIGN(32);
    __audio_work_buffers_end__ = .;
  } > AXI_SRAM

  .ir_buffers (NOLOAD) :
  {
    . = ALIGN(32);
    __ir_buffers_start__ = .;
    KEEP(*(.ir_buffers .ir_buffers.*))
    . = ALIGN(32);
    __ir_buffers_end__ = .;
  } > AXI_SRAM

  .audio_dma_buffers (NOLOAD) :
  {
    . = ALIGN(32);
    __audio_dma_buffers_start__ = .;
    KEEP(*(.audio_dma_buffers .audio_dma_buffers.*))
    . = ALIGN(32);
    __audio_dma_buffers_end__ = .;
  } > DMA_D2

  .display_dma_buffers (NOLOAD) :
  {
    . = ALIGN(32);
    __display_dma_buffers_start__ = .;
    KEEP(*(.display_dma_buffers .display_dma_buffers.*))
    . = ALIGN(32);
    __display_dma_buffers_end__ = .;
  } > DMA_D2

  __dma_d2_region_start__ = ORIGIN(DMA_D2);
  __dma_d2_region_end__ = ORIGIN(DMA_D2) + LENGTH(DMA_D2);

  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data .data.*)
    . = ALIGN(4);
    _edata = .;
  } > AXI_SRAM AT> QSPI

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } > AXI_SRAM

  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > AXI_SRAM

  /DISCARD/ :
  {
    *(.note.GNU-stack)
    *(.comment)
  }
}

ASSERT(ADDR(.qspi_vector) == 0x90010000,
       "XIP vector table must be exactly 0x90010000")
ASSERT(SIZEOF(.model_weights_dtcm) <= LENGTH(DTCM),
       "DTCM model weights exceed 128 KiB")
ASSERT((SIZEOF(.audio_dma_buffers) + SIZEOF(.display_dma_buffers)) <= LENGTH(DMA_D2),
       "DMA buffers exceed the 32 KiB non-cacheable D2 region")
