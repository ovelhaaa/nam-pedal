#include "audio.h"
#include "platform.h"
#include "platform_manifest.h"

int main(void) {
  platform_clock_result_t clock = {0};
  platform_error_t audio_error;
  uint32_t last_audio_report;
  bool dwt_ok;

  if (!platform_verify_xip_boot_contract()) {
    platform_error_led_init();
    platform_fatal(PLATFORM_ERROR_BOOT_CONTRACT);
  }

  /*
   * PRIMASK remains set from the bootloader. HAL may prepare SysTick here, but
   * no interrupt can run until MPU, caches, diagnostics, and handlers are ready.
   */
  platform_hal_init();
  platform_mpu_configure_application();
  platform_enable_caches();
  if (!platform_uart_init()) {
    platform_fatal(PLATFORM_ERROR_UART);
  }
  dwt_ok = platform_dwt_init_and_test();

  clock.requested = PLATFORM_CLOCK_PERFORMANCE_480MHZ;
  clock.active = PLATFORM_CLOCK_PERFORMANCE_480MHZ;
  clock.device_id = (uint16_t)(DBGMCU->IDCODE & 0x0FFFU);
  clock.revision_id = (uint16_t)(DBGMCU->IDCODE >> 16U);
  clock.revision_known = true;
  clock.performance_allowed = true;
  platform_print_diagnostics(&clock);
  platform_uart_write_dec("dwt.cyccnt.test=", dwt_ok ? 1U : 0U);
  platform_uart_write("xip.contract=PASS\r\n");

  audio_error = audio_init();
  if (audio_error != PLATFORM_OK) {
    platform_fatal(audio_error);
  }
  audio_report_configuration();

  __enable_irq();
  audio_error = audio_start();
  if (audio_error != PLATFORM_OK) {
    platform_fatal(audio_error);
  }
  last_audio_report = HAL_GetTick();

  for (;;) {
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_audio_report) >= AUDIO_REPORT_INTERVAL_MS) {
      last_audio_report = now;
      audio_report_stats();
    }
    __WFI();
  }
}
