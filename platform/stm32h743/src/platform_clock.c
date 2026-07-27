#include "platform.h"

#include <string.h>

#define STM32H743_DEVICE_ID 0x0450U
#define STM32H743_REV_Y 0x1003U
#define STM32H743_REV_X 0x2001U
#define STM32H743_REV_V 0x2003U
#define VOS_READY_TIMEOUT_LOOPS 1000000UL

static bool wait_for_vos_ready(void) {
  uint32_t timeout = VOS_READY_TIMEOUT_LOOPS;
  while ((__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY) == 0U) && (timeout > 0U)) {
    --timeout;
  }
  return timeout != 0U;
}

static bool configure_voltage(platform_clock_profile_t profile) {
  if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK) {
    return false;
  }

  /*
   * STM32H74x reaches VOS0 through a settled VOS1 state. The VOS0 HAL macro
   * then enables SYSCFG overdrive; do not collapse these two readiness waits.
   */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  if (!wait_for_vos_ready()) {
    return false;
  }
  if (profile == PLATFORM_CLOCK_PERFORMANCE_480MHZ) {
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    return wait_for_vos_ready();
  }
  return true;
}

static bool configure_pll_and_buses(platform_clock_profile_t profile) {
  RCC_OscInitTypeDef oscillator;
  RCC_ClkInitTypeDef buses;
  memset(&oscillator, 0, sizeof(oscillator));
  memset(&buses, 0, sizeof(buses));

  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  oscillator.HSEState = RCC_HSE_ON;
  oscillator.HSIState = RCC_HSI_OFF;
  oscillator.CSIState = RCC_CSI_OFF;
  oscillator.PLL.PLLState = RCC_PLL_ON;
  oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  oscillator.PLL.PLLM = 5U;
  oscillator.PLL.PLLN =
      (profile == PLATFORM_CLOCK_PERFORMANCE_480MHZ) ? 192U : 160U;
  oscillator.PLL.PLLP = 2U;
  oscillator.PLL.PLLQ = 4U;
  oscillator.PLL.PLLR = 2U;
  oscillator.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  oscillator.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  oscillator.PLL.PLLFRACN = 0U;
  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
    return false;
  }

  buses.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
  buses.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  buses.SYSCLKDivider = RCC_SYSCLK_DIV1;
  buses.AHBCLKDivider = RCC_HCLK_DIV2;
  buses.APB3CLKDivider = RCC_APB3_DIV2;
  buses.APB1CLKDivider = RCC_APB1_DIV2;
  buses.APB2CLKDivider = RCC_APB2_DIV2;
  buses.APB4CLKDivider = RCC_APB4_DIV2;

  /*
   * Four wait states are required for 480/240 MHz at VOS0. Keeping the same
   * conservative latency in the 400/200 MHz diagnostic profile simplifies
   * safe switching and remains within the datasheet table.
   */
  if (HAL_RCC_ClockConfig(&buses, FLASH_LATENCY_4) != HAL_OK) {
    return false;
  }

  __HAL_RCC_CSI_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  HAL_EnableCompensationCell();
  SystemCoreClockUpdate();
  return true;
}

const char *platform_silicon_revision_name(uint16_t revision_id) {
  switch (revision_id) {
    case STM32H743_REV_Y:
      return "Y";
    case STM32H743_REV_X:
      return "X";
    case STM32H743_REV_V:
      return "V";
    default:
      return "unknown";
  }
}

const char *platform_clock_profile_name(platform_clock_profile_t profile) {
  return profile == PLATFORM_CLOCK_PERFORMANCE_480MHZ ? "performance-480"
                                                      : "safe-400";
}

platform_error_t platform_clock_configure(platform_clock_result_t *result) {
  uint32_t idcode;
  platform_clock_profile_t active;
  bool known_revision;
  bool performance_allowed;

  if (result == NULL) {
    return PLATFORM_ERROR_CLOCK;
  }
  memset(result, 0, sizeof(*result));
  idcode = DBGMCU->IDCODE;
  result->device_id = (uint16_t)(idcode & 0x0FFFU);
  result->revision_id = (uint16_t)(idcode >> 16U);

#if defined(CLOCK_PROFILE_SAFE_400MHZ)
  result->requested = PLATFORM_CLOCK_SAFE_400MHZ;
#else
  result->requested = PLATFORM_CLOCK_PERFORMANCE_480MHZ;
#endif

  known_revision = result->revision_id == STM32H743_REV_Y ||
                   result->revision_id == STM32H743_REV_X ||
                   result->revision_id == STM32H743_REV_V;
  performance_allowed = result->device_id == STM32H743_DEVICE_ID &&
                        (result->revision_id == STM32H743_REV_X ||
                         result->revision_id == STM32H743_REV_V);
  result->revision_known = known_revision;
  result->performance_allowed = performance_allowed;

  active = result->requested;
  if (active == PLATFORM_CLOCK_PERFORMANCE_480MHZ && !performance_allowed) {
    active = PLATFORM_CLOCK_SAFE_400MHZ;
  }
  result->active = active;

  if (!configure_voltage(active)) {
    return PLATFORM_ERROR_VOS_NOT_READY;
  }
  return configure_pll_and_buses(active) ? PLATFORM_OK : PLATFORM_ERROR_CLOCK;
}
