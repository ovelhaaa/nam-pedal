#ifndef NAM_PLATFORM_MANIFEST_H
#define NAM_PLATFORM_MANIFEST_H

#include "platform.h"

#define NAM_MANIFEST_MAGIC 0x504D414EU
#define NAM_MANIFEST_VERSION 1U
#define NAM_MANIFEST_VALID_FLAG 0x00000001U
#define NAM_MANIFEST_SIZE 64U

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t header_size;
  uint32_t image_address;
  uint32_t image_size;
  uint32_t image_crc32;
  uint32_t firmware_version;
  uint32_t flags;
  uint32_t vector_table_address;
  uint32_t reserved[7];
  uint32_t header_crc32;
} nam_manifest_t;

_Static_assert(sizeof(nam_manifest_t) == NAM_MANIFEST_SIZE, "manifest must be 64 bytes");

typedef enum {
  MANIFEST_VALID = 0,
  MANIFEST_BAD_HEADER,
  MANIFEST_BAD_STACK,
  MANIFEST_BAD_RESET_VECTOR,
  MANIFEST_BAD_IMAGE_CRC
} manifest_validation_t;

manifest_validation_t platform_manifest_validate(const nam_manifest_t *manifest);
_Noreturn void platform_boot_jump(const nam_manifest_t *manifest);
bool platform_verify_xip_boot_contract(void);

#endif
