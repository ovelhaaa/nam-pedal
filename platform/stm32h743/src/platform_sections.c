#include <stdint.h>

#define SECTION_USED(name) __attribute__((section(name), used, aligned(32)))

uint8_t model_weights_sentinel[32] SECTION_USED(".model_weights_dtcm");
uint8_t nam_ring_sentinel[32] SECTION_USED(".nam_ring_buffers");
uint8_t audio_work_sentinel[32] SECTION_USED(".audio_work_buffers");
uint8_t audio_dma_sentinel[32] SECTION_USED(".audio_dma_buffers");
uint8_t display_dma_sentinel[32] SECTION_USED(".display_dma_buffers");
uint8_t ir_sentinel[32] SECTION_USED(".ir_buffers");
