#include "platform.h"

static void configure_region(uint32_t number, uint32_t base, uint32_t size,
                             uint32_t access, uint32_t disable_exec,
                             uint32_t cacheable, uint32_t bufferable,
                             uint32_t shareable, uint32_t tex) {
  MPU_Region_InitTypeDef region = {0};
  region.Enable = MPU_REGION_ENABLE;
  region.Number = number;
  region.BaseAddress = base;
  region.Size = size;
  region.SubRegionDisable = 0x00U;
  region.TypeExtField = tex;
  region.AccessPermission = access;
  region.DisableExec = disable_exec;
  region.IsShareable = shareable;
  region.IsCacheable = cacheable;
  region.IsBufferable = bufferable;
  HAL_MPU_ConfigRegion(&region);
}

void platform_mpu_configure_application(void) {
  HAL_MPU_Disable();

  configure_region(MPU_REGION_NUMBER0, NAM_QSPI_BASE, MPU_REGION_SIZE_8MB,
                   MPU_REGION_PRIV_RO_URO, MPU_INSTRUCTION_ACCESS_ENABLE,
                   MPU_ACCESS_CACHEABLE, MPU_ACCESS_NOT_BUFFERABLE,
                   MPU_ACCESS_NOT_SHAREABLE, MPU_TEX_LEVEL1);
  configure_region(MPU_REGION_NUMBER1, 0x20000000UL, MPU_REGION_SIZE_128KB,
                   MPU_REGION_FULL_ACCESS, MPU_INSTRUCTION_ACCESS_DISABLE,
                   MPU_ACCESS_CACHEABLE, MPU_ACCESS_BUFFERABLE,
                   MPU_ACCESS_NOT_SHAREABLE, MPU_TEX_LEVEL1);
  configure_region(MPU_REGION_NUMBER2, 0x24000000UL, MPU_REGION_SIZE_512KB,
                   MPU_REGION_FULL_ACCESS, MPU_INSTRUCTION_ACCESS_DISABLE,
                   MPU_ACCESS_CACHEABLE, MPU_ACCESS_BUFFERABLE,
                   MPU_ACCESS_NOT_SHAREABLE, MPU_TEX_LEVEL1);
  configure_region(MPU_REGION_NUMBER3, NAM_DMA_D2_BASE, MPU_REGION_SIZE_32KB,
                   MPU_REGION_FULL_ACCESS, MPU_INSTRUCTION_ACCESS_DISABLE,
                   MPU_ACCESS_NOT_CACHEABLE, MPU_ACCESS_NOT_BUFFERABLE,
                   MPU_ACCESS_SHAREABLE, MPU_TEX_LEVEL0);

  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
  __DSB();
  __ISB();
}

void platform_enable_caches(void) {
  SCB_EnableICache();
  SCB_EnableDCache();
}

void platform_disable_caches_and_mpu(void) {
  __disable_irq();
  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
    SCB_CleanInvalidateDCache();
    SCB_DisableDCache();
  }
  if ((SCB->CCR & SCB_CCR_IC_Msk) != 0U) {
    SCB_DisableICache();
  }
  HAL_MPU_Disable();
  __DSB();
  __ISB();
}

void platform_print_memory_map(void) {
  platform_uart_write_hex("model_weights_dtcm.start=",
                          (uint32_t)&__model_weights_dtcm_start__);
  platform_uart_write_hex("model_weights_dtcm.end=",
                          (uint32_t)&__model_weights_dtcm_end__);
  platform_uart_write_hex("nam_ring_buffers.start=",
                          (uint32_t)&__nam_ring_buffers_start__);
  platform_uart_write_hex("audio_work_buffers.start=",
                          (uint32_t)&__audio_work_buffers_start__);
  platform_uart_write_hex("audio_dma_buffers.start=",
                          (uint32_t)&__audio_dma_buffers_start__);
  platform_uart_write_hex("display_dma_buffers.start=",
                          (uint32_t)&__display_dma_buffers_start__);
  platform_uart_write_hex("ir_buffers.start=", (uint32_t)&__ir_buffers_start__);
  platform_uart_write_hex("dma_d2_region.start=",
                          (uint32_t)&__dma_d2_region_start__);
  platform_uart_write_hex("dma_d2_region.end=",
                          (uint32_t)&__dma_d2_region_end__);
}
