#include "platform.h"
#include "platform_qspi.h"

int main(void) {
  platform_clock_result_t clock;
  platform_qspi_result_t qspi;
  platform_error_t clock_error;
  bool dwt_ok;

  platform_hal_init();
  platform_error_led_init();
  clock_error = platform_clock_configure(&clock);
  if (clock_error != PLATFORM_OK) {
    platform_fatal(clock_error);
  }
  if (!platform_uart_init()) {
    platform_fatal(PLATFORM_ERROR_UART);
  }

  dwt_ok = platform_dwt_init_and_test();
  platform_mpu_configure_application();
  platform_enable_caches();
  platform_print_diagnostics(&clock);
  platform_uart_write_dec("dwt.cyccnt.test=", dwt_ok ? 1U : 0U);

  if (!platform_qspi_bring_up_and_report(&qspi)) {
    platform_fatal(qspi.error);
  }
  platform_uart_write("bringup.result=PASS\r\n");

  for (;;) {
    __WFI();
  }
}
