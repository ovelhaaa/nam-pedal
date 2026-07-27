#include "platform.h"

void platform_error_led_init(void) {
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOE_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_3;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &gpio);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);
}

_Noreturn void platform_fatal(platform_error_t error) {
  uint32_t pulses = (uint32_t)error;
  if (pulses == 0U) {
    pulses = 1U;
  }
  platform_uart_write_hex("FATAL=", (uint32_t)error);
  __disable_irq();

  for (;;) {
    for (uint32_t index = 0U; index < pulses; ++index) {
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);
      for (volatile uint32_t delay = 0U; delay < 600000U; ++delay) {
        __NOP();
      }
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);
      for (volatile uint32_t delay = 0U; delay < 600000U; ++delay) {
        __NOP();
      }
    }
    for (volatile uint32_t delay = 0U; delay < 2400000U; ++delay) {
      __NOP();
    }
  }
}
