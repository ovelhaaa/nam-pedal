#include "platform_manifest.h"

uint32_t platform_crc32(const void *data, size_t size) {
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t index = 0U; index < size; ++index) {
    crc ^= bytes[index];
    for (uint32_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

static bool stack_pointer_valid(uint32_t stack_pointer) {
  if ((stack_pointer & 0x7U) != 0U) {
    return false;
  }
  return (stack_pointer >= 0x20000000UL && stack_pointer <= 0x20020000UL) ||
         (stack_pointer >= 0x24000000UL && stack_pointer <= 0x24080000UL) ||
         (stack_pointer >= 0x30000000UL && stack_pointer <= 0x30048000UL) ||
         (stack_pointer >= 0x38000000UL && stack_pointer <= 0x38010000UL);
}

manifest_validation_t platform_manifest_validate(
    const nam_manifest_t *manifest) {
  const uint32_t *vectors;
  uint32_t reset_address;
  if (manifest == NULL || manifest->magic != NAM_MANIFEST_MAGIC ||
      manifest->version != NAM_MANIFEST_VERSION ||
      manifest->header_size != NAM_MANIFEST_SIZE ||
      manifest->image_address != NAM_XIP_APPLICATION_ADDRESS ||
      manifest->vector_table_address != NAM_XIP_APPLICATION_ADDRESS ||
      manifest->image_size < 8U ||
      manifest->image_size > (NAM_QSPI_SIZE - NAM_QSPI_METADATA_SIZE) ||
      (manifest->flags & NAM_MANIFEST_VALID_FLAG) == 0U ||
      platform_crc32(manifest, NAM_MANIFEST_SIZE - sizeof(uint32_t)) !=
          manifest->header_crc32) {
    return MANIFEST_BAD_HEADER;
  }

  vectors = (const uint32_t *)manifest->vector_table_address;
  if (!stack_pointer_valid(vectors[0])) {
    return MANIFEST_BAD_STACK;
  }
  reset_address = vectors[1];
  if ((reset_address & 1U) == 0U ||
      (reset_address & ~1U) < manifest->image_address ||
      (reset_address & ~1U) >=
          (manifest->image_address + manifest->image_size)) {
    return MANIFEST_BAD_RESET_VECTOR;
  }
  if (platform_crc32((const void *)manifest->image_address,
                     manifest->image_size) != manifest->image_crc32) {
    return MANIFEST_BAD_IMAGE_CRC;
  }
  return MANIFEST_VALID;
}

_Noreturn void platform_boot_jump(const nam_manifest_t *manifest) {
  typedef void (*entry_point_t)(void);
  const uint32_t *vectors =
      (const uint32_t *)manifest->vector_table_address;
  entry_point_t entry = (entry_point_t)vectors[1];

  __disable_irq();
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
  for (uint32_t bank = 0U; bank < 8U; ++bank) {
    NVIC->ICER[bank] = 0xFFFFFFFFU;
    NVIC->ICPR[bank] = 0xFFFFFFFFU;
  }
  uint32_t uart_timeout = 200000U;
  while (((USART3->ISR & USART_ISR_TC) == 0U) && (uart_timeout > 0U)) {
    --uart_timeout;
  }
  CLEAR_BIT(USART3->CR1, USART_CR1_UE);
  __HAL_RCC_USART3_CLK_DISABLE();

  platform_disable_caches_and_mpu();
  SCB->VTOR = manifest->vector_table_address;
  __set_CONTROL(0U);
  __set_MSP(vectors[0]);
  __DSB();
  __ISB();
  entry();
  platform_fatal(PLATFORM_ERROR_RESET_VECTOR);
}

bool platform_verify_xip_boot_contract(void) {
  SystemCoreClockUpdate();
  return SystemCoreClock == 480000000U &&
         HAL_RCC_GetHCLKFreq() == 240000000U &&
         __HAL_RCC_QSPI_IS_CLK_ENABLED() &&
         ((QUADSPI->CCR & QUADSPI_CCR_FMODE) == QUADSPI_CCR_FMODE) &&
         (SCB->CCR & (SCB_CCR_IC_Msk | SCB_CCR_DC_Msk)) == 0U &&
         (MPU->CTRL & MPU_CTRL_ENABLE_Msk) == 0U && SysTick->CTRL == 0U &&
         (SCB->ICSR &
          (SCB_ICSR_PENDSTSET_Msk | SCB_ICSR_PENDSVSET_Msk)) == 0U &&
         SCB->VTOR == NAM_XIP_APPLICATION_ADDRESS &&
         __get_PRIMASK() != 0U;
}
