#include "platform.h"
#include "platform_manifest.h"

int main(void) {
  platform_clock_result_t clock = {0};
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

  __enable_irq();
  for (;;) {
    __WFI();
  }
}
