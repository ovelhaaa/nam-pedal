#ifndef NAM_PLATFORM_H
#define NAM_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32h7xx_hal.h"

#define NAM_FIRMWARE_NAME "nam-pedal platform foundation"
#define NAM_FIRMWARE_VERSION "0.1.0"

#define NAM_INTERNAL_FLASH_BASE 0x08000000UL
#define NAM_BOOTLOADER_SIZE 0x00020000UL
#define NAM_QSPI_BASE 0x90000000UL
#define NAM_QSPI_SIZE 0x00800000UL
#define NAM_QSPI_METADATA_SIZE 0x00010000UL
#define NAM_XIP_APPLICATION_ADDRESS (NAM_QSPI_BASE + NAM_QSPI_METADATA_SIZE)
#define NAM_DMA_D2_BASE 0x30000000UL
#define NAM_DMA_D2_SIZE 0x00008000UL

typedef enum {
  PLATFORM_OK = 0,
  PLATFORM_ERROR_CLOCK = 1,
  PLATFORM_ERROR_VOS_NOT_READY = 2,
  PLATFORM_ERROR_UART = 3,
  PLATFORM_ERROR_JEDEC_UNKNOWN = 4,
  PLATFORM_ERROR_QSPI_INDIRECT_READ = 5,
  PLATFORM_ERROR_QSPI_MEMORY_MAPPED = 6,
  PLATFORM_ERROR_MANIFEST = 7,
  PLATFORM_ERROR_STACK_POINTER = 8,
  PLATFORM_ERROR_RESET_VECTOR = 9,
  PLATFORM_ERROR_CRC = 10,
  PLATFORM_ERROR_BOOT_CONTRACT = 11,
  PLATFORM_ERROR_QSPI_CONFIG = 12,
  PLATFORM_ERROR_AUDIO_CLOCK = 13,
  PLATFORM_ERROR_AUDIO_SAI = 14,
  PLATFORM_ERROR_AUDIO_DMA = 15
} platform_error_t;

typedef enum {
  PLATFORM_CLOCK_SAFE_400MHZ = 400,
  PLATFORM_CLOCK_PERFORMANCE_480MHZ = 480
} platform_clock_profile_t;

typedef struct {
  platform_clock_profile_t requested;
  platform_clock_profile_t active;
  uint16_t device_id;
  uint16_t revision_id;
  bool revision_known;
  bool performance_allowed;
} platform_clock_result_t;

void platform_hal_init(void);
platform_error_t platform_clock_configure(platform_clock_result_t *result);
const char *platform_clock_profile_name(platform_clock_profile_t profile);
const char *platform_silicon_revision_name(uint16_t revision_id);

bool platform_uart_init(void);
bool platform_uart_is_ready(void);
void platform_uart_write(const char *text);
void platform_uart_write_hex(const char *label, uint32_t value);
void platform_uart_write_dec(const char *label, uint32_t value);

bool platform_dwt_init_and_test(void);
void platform_dwt_delay_cycles(uint32_t cycles);

void platform_mpu_configure_application(void);
void platform_enable_caches(void);
void platform_disable_caches_and_mpu(void);
void platform_print_memory_map(void);

void platform_error_led_init(void);
_Noreturn void platform_fatal(platform_error_t error);

uint32_t platform_crc32(const void *data, size_t size);
void platform_print_diagnostics(const platform_clock_result_t *clock_result);

extern uint8_t __model_weights_dtcm_start__;
extern uint8_t __model_weights_dtcm_end__;
extern uint8_t __nam_ring_buffers_start__;
extern uint8_t __nam_ring_buffers_end__;
extern uint8_t __audio_work_buffers_start__;
extern uint8_t __audio_work_buffers_end__;
extern uint8_t __audio_dma_buffers_start__;
extern uint8_t __audio_dma_buffers_end__;
extern uint8_t __display_dma_buffers_start__;
extern uint8_t __display_dma_buffers_end__;
extern uint8_t __ir_buffers_start__;
extern uint8_t __ir_buffers_end__;
extern uint8_t __dma_d2_region_start__;
extern uint8_t __dma_d2_region_end__;

#endif
