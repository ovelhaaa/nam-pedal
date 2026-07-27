_estack = ORIGIN(AXI_SRAM) + LENGTH(AXI_SRAM);
_Min_Heap_Size = 0x000;
_Min_Stack_Size = 0x1000;

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(1024);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > ROM

  .text :
  {
    . = ALIGN(4);
    *(.text .text.*)
    *(.glue_7) *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } > ROM

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata .rodata.*)
    . = ALIGN(4);
  } > ROM

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } > ROM

  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > ROM

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > ROM

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > ROM

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > ROM

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
  } > RAM

  .audio_work_buffers (NOLOAD) :
  {
    . = ALIGN(32);
    __audio_work_buffers_start__ = .;
    KEEP(*(.audio_work_buffers .audio_work_buffers.*))
    . = ALIGN(32);
    __audio_work_buffers_end__ = .;
  } > RAM

  .ir_buffers (NOLOAD) :
  {
    . = ALIGN(32);
    __ir_buffers_start__ = .;
    KEEP(*(.ir_buffers .ir_buffers.*))
    . = ALIGN(32);
    __ir_buffers_end__ = .;
  } > RAM

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
  } > RAM AT> ROM

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
  } > RAM

  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > RAM

  /DISCARD/ :
  {
    *(.note.GNU-stack)
    *(.comment)
  }
}

ASSERT(SIZEOF(.model_weights_dtcm) <= LENGTH(DTCM),
       "DTCM model weights exceed 128 KiB")
ASSERT(SIZEOF(.isr_vector) <= 1024,
       "Interrupt vector table exceeds its 1024-byte alignment")
ASSERT((SIZEOF(.audio_dma_buffers) + SIZEOF(.display_dma_buffers)) <= LENGTH(DMA_D2),
       "DMA buffers exceed the 32 KiB non-cacheable D2 region")
