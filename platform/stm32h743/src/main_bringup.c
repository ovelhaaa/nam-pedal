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

  if (!platform_qspi_initialize(&qspi)) {
    platform_uart_write_hex("qspi.jedec=",
                            ((uint32_t)qspi.jedec_id[0] << 16U) |
                                ((uint32_t)qspi.jedec_id[1] << 8U) |
                                qspi.jedec_id[2]);
    platform_fatal(qspi.error);
  }
  platform_uart_write_hex("qspi.jedec=",
                          ((uint32_t)qspi.jedec_id[0] << 16U) |
                              ((uint32_t)qspi.jedec_id[1] << 8U) |
                              qspi.jedec_id[2]);
  platform_uart_write("qspi.device=");
  platform_uart_write(qspi.device->name);
  platform_uart_write("\r\n");
  platform_uart_write_hex("qspi.indirect.crc32=", qspi.indirect_crc32);
  if (!platform_qspi_enter_memory_mapped(&qspi)) {
    platform_fatal(qspi.error);
  }
  platform_uart_write_hex("qspi.mapped.crc32=", qspi.mapped_crc32);
  platform_uart_write("bringup.result=PASS\r\n");

  for (;;) {
    __WFI();
  }
}
