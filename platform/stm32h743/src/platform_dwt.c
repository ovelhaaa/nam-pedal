#include "platform.h"

bool platform_dwt_init_and_test(void) {
  uint32_t before;
  uint32_t after;
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->LAR = 0xC5ACCE55U;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  before = DWT->CYCCNT;
  __NOP();
  __NOP();
  __NOP();
  after = DWT->CYCCNT;
  return after > before;
}

void platform_dwt_delay_cycles(uint32_t cycles) {
  uint32_t start = DWT->CYCCNT;
  while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
    __NOP();
  }
}
