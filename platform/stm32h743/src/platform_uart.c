#include "platform.h"

#define UART_TX_TIMEOUT_LOOPS 200000UL

static UART_HandleTypeDef debug_uart;
static bool debug_uart_ready;

void HAL_UART_MspInit(UART_HandleTypeDef *handle) {
  GPIO_InitTypeDef gpio = {0};
  if (handle->Instance != USART3) {
    return;
  }

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_USART3_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOD, &gpio);
}

bool platform_uart_init(void) {
  debug_uart.Instance = USART3;
  debug_uart.Init.BaudRate = 115200U;
  debug_uart.Init.WordLength = UART_WORDLENGTH_8B;
  debug_uart.Init.StopBits = UART_STOPBITS_1;
  debug_uart.Init.Parity = UART_PARITY_NONE;
  debug_uart.Init.Mode = UART_MODE_TX_RX;
  debug_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  debug_uart.Init.OverSampling = UART_OVERSAMPLING_16;
  debug_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  debug_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  debug_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  debug_uart_ready = HAL_UART_Init(&debug_uart) == HAL_OK;
  return debug_uart_ready;
}

bool platform_uart_is_ready(void) {
  return debug_uart_ready;
}

static bool write_byte(uint8_t byte) {
  uint32_t timeout = UART_TX_TIMEOUT_LOOPS;
  if (!debug_uart_ready) {
    return false;
  }
  while ((__HAL_UART_GET_FLAG(&debug_uart, UART_FLAG_TXE) == 0U) &&
         (timeout > 0U)) {
    --timeout;
  }
  if (timeout == 0U) {
    return false;
  }
  debug_uart.Instance->TDR = byte;
  return true;
}

void platform_uart_write(const char *text) {
  if (text == NULL) {
    return;
  }
  while (*text != '\0') {
    if (!write_byte((uint8_t)*text)) {
      debug_uart_ready = false;
      return;
    }
    ++text;
  }
}

void platform_uart_write_hex(const char *label, uint32_t value) {
  static const char hex[] = "0123456789ABCDEF";
  char buffer[11] = "0x00000000";
  for (uint32_t index = 0U; index < 8U; ++index) {
    buffer[9U - index] = hex[value & 0xFU];
    value >>= 4U;
  }
  platform_uart_write(label);
  platform_uart_write(buffer);
  platform_uart_write("\r\n");
}

void platform_uart_write_dec(const char *label, uint32_t value) {
  char reverse[10];
  char output[11];
  uint32_t count = 0U;
  uint32_t out = 0U;

  do {
    reverse[count++] = (char)('0' + (value % 10U));
    value /= 10U;
  } while ((value != 0U) && (count < sizeof(reverse)));
  while (count > 0U) {
    output[out++] = reverse[--count];
  }
  output[out] = '\0';
  platform_uart_write(label);
  platform_uart_write(output);
  platform_uart_write("\r\n");
}
