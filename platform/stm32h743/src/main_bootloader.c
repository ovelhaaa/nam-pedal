#include "platform.h"
#include "platform_manifest.h"
#include "platform_qspi.h"

static platform_error_t manifest_error(manifest_validation_t validation) {
  switch (validation) {
    case MANIFEST_BAD_STACK:
      return PLATFORM_ERROR_STACK_POINTER;
    case MANIFEST_BAD_RESET_VECTOR:
      return PLATFORM_ERROR_RESET_VECTOR;
    case MANIFEST_BAD_IMAGE_CRC:
      return PLATFORM_ERROR_CRC;
    case MANIFEST_BAD_HEADER:
    default:
      return PLATFORM_ERROR_MANIFEST;
  }
}

int main(void) {
  const nam_manifest_t *manifest = (const nam_manifest_t *)NAM_QSPI_BASE;
  platform_clock_result_t clock;
  platform_qspi_result_t qspi;
  platform_error_t clock_error;
  manifest_validation_t validation;

  platform_hal_init();
  platform_error_led_init();
  clock_error = platform_clock_configure(&clock);
  if (clock_error != PLATFORM_OK) {
    platform_fatal(clock_error);
  }
  if (!platform_uart_init()) {
    platform_fatal(PLATFORM_ERROR_UART);
  }
  (void)platform_dwt_init_and_test();
  platform_print_diagnostics(&clock);

  if (clock.active != PLATFORM_CLOCK_PERFORMANCE_480MHZ) {
    platform_uart_write("boot.refused=480MHz unsupported by silicon revision\r\n");
    platform_fatal(PLATFORM_ERROR_CLOCK);
  }
  if (!platform_qspi_bring_up_and_report(&qspi)) {
    platform_fatal(qspi.error);
  }

  validation = platform_manifest_validate(manifest);
  if (validation != MANIFEST_VALID) {
    platform_fatal(manifest_error(validation));
  }
  platform_uart_write("manifest=VALID\r\njump=0x90010000\r\n");
  platform_boot_jump(manifest);
}
