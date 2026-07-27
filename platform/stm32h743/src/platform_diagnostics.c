#include "platform.h"
#include "platform_qspi.h"

static uint32_t qspi_jedec_id(const platform_qspi_result_t *result) {
  return ((uint32_t)result->jedec_id[0] << 16U) |
         ((uint32_t)result->jedec_id[1] << 8U) | result->jedec_id[2];
}

bool platform_qspi_bring_up_and_report(platform_qspi_result_t *result) {
  if (!platform_qspi_initialize(result)) {
    platform_uart_write_hex("qspi.jedec=", qspi_jedec_id(result));
    return false;
  }

  platform_uart_write_hex("qspi.jedec=", qspi_jedec_id(result));
  platform_uart_write("qspi.device=");
  platform_uart_write(result->device->name);
  platform_uart_write("\r\n");
  platform_uart_write_hex("qspi.indirect.crc32=", result->indirect_crc32);
  if (!platform_qspi_enter_memory_mapped(result)) {
    return false;
  }
  platform_uart_write_hex("qspi.mapped.crc32=", result->mapped_crc32);
  return true;
}

void platform_print_diagnostics(const platform_clock_result_t *clock_result) {
  uint32_t idcode = DBGMCU->IDCODE;
  platform_uart_write("\r\n");
  platform_uart_write(NAM_FIRMWARE_NAME " " NAM_FIRMWARE_VERSION "\r\n");
  platform_uart_write("clock.requested=");
  platform_uart_write(platform_clock_profile_name(clock_result->requested));
  platform_uart_write("\r\nclock.active=");
  platform_uart_write(platform_clock_profile_name(clock_result->active));
  platform_uart_write("\r\nsilicon.revision=");
  platform_uart_write(platform_silicon_revision_name(clock_result->revision_id));
  platform_uart_write("\r\n");
  platform_uart_write_dec("sysclk=", HAL_RCC_GetSysClockFreq());
  platform_uart_write_dec("hclk=", HAL_RCC_GetHCLKFreq());
  platform_uart_write_dec("apb1=", HAL_RCC_GetPCLK1Freq());
  platform_uart_write_dec("apb2=", HAL_RCC_GetPCLK2Freq());
  platform_uart_write_dec("apb3=", HAL_RCCEx_GetD1PCLK1Freq());
  platform_uart_write_dec("apb4=", HAL_RCCEx_GetD3PCLK1Freq());
  platform_uart_write_dec("SystemCoreClock=", SystemCoreClock);
  platform_uart_write_hex("DBGMCU.IDCODE=", idcode);
  platform_uart_write_hex("DEV_ID=", idcode & 0x0FFFU);
  platform_uart_write_hex("REV_ID=", idcode >> 16U);
  platform_uart_write_hex("VTOR=", SCB->VTOR);
  platform_uart_write_hex("MSP=", __get_MSP());
  platform_uart_write_dec("icache=",
                          (SCB->CCR & SCB_CCR_IC_Msk) != 0U ? 1U : 0U);
  platform_uart_write_dec("dcache=",
                          (SCB->CCR & SCB_CCR_DC_Msk) != 0U ? 1U : 0U);
  platform_uart_write_dec("mpu=",
                          (MPU->CTRL & MPU_CTRL_ENABLE_Msk) != 0U ? 1U : 0U);
  platform_print_memory_map();
}
