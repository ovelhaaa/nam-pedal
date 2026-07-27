#ifndef NAM_PLATFORM_QSPI_H
#define NAM_PLATFORM_QSPI_H

#include "platform.h"

typedef struct {
  uint8_t manufacturer;
  uint8_t memory_type;
  uint8_t capacity;
  const char *name;
  uint8_t quad_io_read_opcode;
  uint8_t status2_read_opcode;
  uint8_t status2_write_opcode;
  uint8_t qe_mask;
  uint8_t mode_bits;
  uint8_t dummy_cycles;
} platform_qspi_device_t;

typedef struct {
  uint8_t jedec_id[3];
  const platform_qspi_device_t *device;
  uint32_t indirect_crc32;
  uint32_t mapped_crc32;
  bool memory_mapped;
  platform_error_t error;
} platform_qspi_result_t;

bool platform_qspi_initialize(platform_qspi_result_t *result);
bool platform_qspi_enter_memory_mapped(platform_qspi_result_t *result);
bool platform_qspi_bring_up_and_report(platform_qspi_result_t *result);
bool platform_qspi_is_memory_mapped(void);
const platform_qspi_device_t *platform_qspi_lookup(const uint8_t jedec_id[3]);

#endif
