#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"

uint32_t SystemCoreClock = HSI_VALUE;
uint32_t SystemD2Clock = HSI_VALUE;
const uint8_t D1CorePrescTable[16] = {
    0, 0, 0, 0, 1, 2, 3, 4, 1, 2, 3, 4, 6, 7, 8, 9};

extern uint32_t g_pfnVectors;

/*
 * Deliberately does not reset RCC. The XIP image inherits an active QSPI
 * mapping from the bootloader, and a global clock reset would fetch-fault.
 */
void SystemInit(void) {
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  SCB->CPACR |= (3UL << (10U * 2U)) | (3UL << (11U * 2U));
#endif
  SCB->VTOR = (uint32_t)&g_pfnVectors;
  __DSB();
  __ISB();
}

void ExitRun0Mode(void) {
  /* Supply/clock policy is established in platform_clock_configure(). */
}

void SystemCoreClockUpdate(void) {
  uint32_t system_clock = HAL_RCC_GetSysClockFreq();
  uint32_t d1_shift =
      D1CorePrescTable[(RCC->D1CFGR & RCC_D1CFGR_D1CPRE) >>
                       RCC_D1CFGR_D1CPRE_Pos] &
      0x1FU;
  uint32_t hclk_shift =
      D1CorePrescTable[(RCC->D1CFGR & RCC_D1CFGR_HPRE) >>
                       RCC_D1CFGR_HPRE_Pos] &
      0x1FU;
  SystemCoreClock = system_clock >> d1_shift;
  SystemD2Clock = SystemCoreClock >> hclk_shift;
}
