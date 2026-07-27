#include "platform.h"

void platform_hal_init(void) {
  HAL_Init();
}

void SysTick_Handler(void) {
  HAL_IncTick();
}

void NMI_Handler(void) {}

void HardFault_Handler(void) {
  platform_fatal(PLATFORM_ERROR_BOOT_CONTRACT);
}

void MemManage_Handler(void) {
  platform_fatal(PLATFORM_ERROR_BOOT_CONTRACT);
}

void BusFault_Handler(void) {
  platform_fatal(PLATFORM_ERROR_BOOT_CONTRACT);
}

void UsageFault_Handler(void) {
  platform_fatal(PLATFORM_ERROR_BOOT_CONTRACT);
}

void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

void HAL_MspInit(void) {
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}
