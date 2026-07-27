#include "platform_qspi.h"

#include <string.h>

#define WINBOND_RESET_ENABLE 0x66U
#define WINBOND_RESET_DEVICE 0x99U
#define WINBOND_READ_JEDEC_ID 0x9FU
#define WINBOND_WRITE_VOLATILE_STATUS_ENABLE 0x50U
#define WINBOND_READ_STATUS1 0x05U
#define QSPI_TEST_BYTES 256U
#define QSPI_COMMAND_TIMEOUT_MS 100U
#define QSPI_BUSY_POLLS 512U

static QSPI_HandleTypeDef qspi;
static bool qspi_memory_mapped;

static const platform_qspi_device_t supported_devices[] = {
    {0xEFU, 0x40U, 0x17U, "Winbond W25Q64JV (EF4017)", 0xEBU, 0x35U, 0x31U,
     0x02U, 0xF0U, 4U},
    {0xEFU, 0x70U, 0x17U, "Winbond W25Q64JV-IQ/JQ (EF7017)", 0xEBU, 0x35U,
     0x31U, 0x02U, 0xF0U, 4U},
};

static void command_defaults(QSPI_CommandTypeDef *command) {
  memset(command, 0, sizeof(*command));
  command->InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command->AddressMode = QSPI_ADDRESS_NONE;
  command->AddressSize = QSPI_ADDRESS_24_BITS;
  command->AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command->AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
  command->DataMode = QSPI_DATA_NONE;
  command->DdrMode = QSPI_DDR_MODE_DISABLE;
  command->DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command->SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
}

void HAL_QSPI_MspInit(QSPI_HandleTypeDef *handle) {
  RCC_PeriphCLKInitTypeDef clock = {0};
  GPIO_InitTypeDef gpio = {0};
  if (handle->Instance != QUADSPI) {
    return;
  }

  clock.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
  clock.QspiClockSelection = RCC_QSPICLKSOURCE_PLL2;
  clock.PLL2.PLL2M = 5U;
  clock.PLL2.PLL2N = 96U;
  clock.PLL2.PLL2P = 2U;
  clock.PLL2.PLL2Q = 2U;
  clock.PLL2.PLL2R = 4U;
  clock.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
  clock.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  clock.PLL2.PLL2FRACN = 0U;
  if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) {
    return;
  }

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_QSPI_CLK_ENABLE();
  __HAL_RCC_QSPI_FORCE_RESET();
  __HAL_RCC_QSPI_RELEASE_RESET();

  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

  gpio.Pin = GPIO_PIN_2;
  gpio.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(GPIOB, &gpio);
  gpio.Pin = GPIO_PIN_6;
  gpio.Alternate = GPIO_AF10_QUADSPI;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
  gpio.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(GPIOD, &gpio);

  gpio.Pin = GPIO_PIN_2;
  gpio.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(GPIOE, &gpio);
}

const platform_qspi_device_t *platform_qspi_lookup(
    const uint8_t jedec_id[3]) {
  for (size_t index = 0U;
       index < sizeof(supported_devices) / sizeof(supported_devices[0]);
       ++index) {
    const platform_qspi_device_t *device = &supported_devices[index];
    if (device->manufacturer == jedec_id[0] &&
        device->memory_type == jedec_id[1] &&
        device->capacity == jedec_id[2]) {
      return device;
    }
  }
  return NULL;
}

static bool send_reset(uint32_t instruction_mode) {
  QSPI_CommandTypeDef command;
  command_defaults(&command);
  command.InstructionMode = instruction_mode;
  command.Instruction = WINBOND_RESET_ENABLE;
  if (HAL_QSPI_Command(&qspi, &command, QSPI_COMMAND_TIMEOUT_MS) != HAL_OK) {
    return false;
  }
  command.Instruction = WINBOND_RESET_DEVICE;
  if (HAL_QSPI_Command(&qspi, &command, QSPI_COMMAND_TIMEOUT_MS) != HAL_OK) {
    return false;
  }
  for (volatile uint32_t delay = 0U; delay < (SystemCoreClock / 40000U);
       ++delay) {
    __NOP();
  }
  return true;
}

static bool read_register(uint8_t opcode, uint8_t *value) {
  QSPI_CommandTypeDef command;
  command_defaults(&command);
  command.Instruction = opcode;
  command.DataMode = QSPI_DATA_1_LINE;
  command.NbData = 1U;
  return HAL_QSPI_Command(&qspi, &command, QSPI_COMMAND_TIMEOUT_MS) == HAL_OK &&
         HAL_QSPI_Receive(&qspi, value, QSPI_COMMAND_TIMEOUT_MS) == HAL_OK;
}

static bool wait_while_busy(void) {
  for (uint32_t poll = 0U; poll < QSPI_BUSY_POLLS; ++poll) {
    uint8_t status;
    if (!read_register(WINBOND_READ_STATUS1, &status)) {
      return false;
    }
    if ((status & 0x01U) == 0U) {
      return true;
    }
  }
  return false;
}

static bool configure_quad_enable(const platform_qspi_device_t *device) {
  QSPI_CommandTypeDef command;
  uint8_t status2;
  if (!read_register(device->status2_read_opcode, &status2)) {
    return false;
  }
  if ((status2 & device->qe_mask) != 0U) {
    return true;
  }

  command_defaults(&command);
  command.Instruction = WINBOND_WRITE_VOLATILE_STATUS_ENABLE;
  if (HAL_QSPI_Command(&qspi, &command, QSPI_COMMAND_TIMEOUT_MS) != HAL_OK) {
    return false;
  }

  status2 |= device->qe_mask;
  command.Instruction = device->status2_write_opcode;
  command.DataMode = QSPI_DATA_1_LINE;
  command.NbData = 1U;
  if (HAL_QSPI_Command(&qspi, &command, QSPI_COMMAND_TIMEOUT_MS) != HAL_OK ||
      HAL_QSPI_Transmit(&qspi, &status2, QSPI_COMMAND_TIMEOUT_MS) != HAL_OK ||
      !wait_while_busy() ||
      !read_register(device->status2_read_opcode, &status2)) {
    return false;
  }
  return (status2 & device->qe_mask) != 0U;
}

static void quad_io_read_command(QSPI_CommandTypeDef *command,
                                 const platform_qspi_device_t *device,
                                 uint32_t bytes) {
  command_defaults(command);
  command->Instruction = device->quad_io_read_opcode;
  command->Address = 0U;
  command->AddressMode = QSPI_ADDRESS_4_LINES;
  command->AlternateByteMode = QSPI_ALTERNATE_BYTES_4_LINES;
  command->AlternateBytes = device->mode_bits;
  command->DataMode = QSPI_DATA_4_LINES;
  command->DummyCycles = device->dummy_cycles;
  command->NbData = bytes;
}

bool platform_qspi_initialize(platform_qspi_result_t *result) {
  QSPI_CommandTypeDef command;
  uint8_t test[QSPI_TEST_BYTES];
  if (result == NULL) {
    return false;
  }
  memset(result, 0, sizeof(*result));
  qspi_memory_mapped = false;

  qspi.Instance = QUADSPI;
  qspi.Init.ClockPrescaler = 1U; /* PLL2R 120 MHz / (1 + 1) = 60 MHz. */
  qspi.Init.FifoThreshold = 4U;
  qspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
  qspi.Init.FlashSize = 22U; /* 2^(22 + 1) = 8 MiB. */
  qspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_3_CYCLE;
  qspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
  qspi.Init.FlashID = QSPI_FLASH_ID_1;
  qspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
  if (HAL_QSPI_Init(&qspi) != HAL_OK) {
    result->error = PLATFORM_ERROR_QSPI_CONFIG;
    return false;
  }

  /*
   * Issue reset sequences in both possible instruction widths. Failure in the
   * four-line recovery attempt is harmless; the mandatory one-line reset and
   * JEDEC read below decide whether communication is valid.
   */
  (void)send_reset(QSPI_INSTRUCTION_4_LINES);
  if (!send_reset(QSPI_INSTRUCTION_1_LINE)) {
    result->error = PLATFORM_ERROR_QSPI_CONFIG;
    return false;
  }

  command_defaults(&command);
  command.Instruction = WINBOND_READ_JEDEC_ID;
  command.DataMode = QSPI_DATA_1_LINE;
  command.NbData = sizeof(result->jedec_id);
  if (HAL_QSPI_Command(&qspi, &command, QSPI_COMMAND_TIMEOUT_MS) != HAL_OK ||
      HAL_QSPI_Receive(&qspi, result->jedec_id, QSPI_COMMAND_TIMEOUT_MS) !=
          HAL_OK) {
    result->error = PLATFORM_ERROR_QSPI_INDIRECT_READ;
    return false;
  }

  result->device = platform_qspi_lookup(result->jedec_id);
  if (result->device == NULL) {
    result->error = PLATFORM_ERROR_JEDEC_UNKNOWN;
    return false;
  }
  if (!configure_quad_enable(result->device)) {
    result->error = PLATFORM_ERROR_QSPI_CONFIG;
    return false;
  }

  quad_io_read_command(&command, result->device, sizeof(test));
  if (HAL_QSPI_Command(&qspi, &command, QSPI_COMMAND_TIMEOUT_MS) != HAL_OK ||
      HAL_QSPI_Receive(&qspi, test, QSPI_COMMAND_TIMEOUT_MS) != HAL_OK) {
    result->error = PLATFORM_ERROR_QSPI_INDIRECT_READ;
    return false;
  }
  result->indirect_crc32 = platform_crc32(test, sizeof(test));
  return true;
}

bool platform_qspi_enter_memory_mapped(platform_qspi_result_t *result) {
  QSPI_CommandTypeDef command;
  QSPI_MemoryMappedTypeDef mapped = {0};
  uint8_t test[QSPI_TEST_BYTES];
  volatile const uint8_t *source = (volatile const uint8_t *)NAM_QSPI_BASE;

  if (result == NULL || result->device == NULL) {
    return false;
  }
  quad_io_read_command(&command, result->device, 0U);
  mapped.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
  mapped.TimeOutPeriod = 0U;
  if (HAL_QSPI_MemoryMapped(&qspi, &command, &mapped) != HAL_OK) {
    result->error = PLATFORM_ERROR_QSPI_MEMORY_MAPPED;
    return false;
  }
  qspi_memory_mapped = true;
  __DSB();
  __ISB();
  for (size_t index = 0U; index < sizeof(test); ++index) {
    test[index] = source[index];
  }
  result->mapped_crc32 = platform_crc32(test, sizeof(test));
  result->memory_mapped = true;
  if (result->mapped_crc32 != result->indirect_crc32) {
    result->error = PLATFORM_ERROR_CRC;
    return false;
  }
  result->error = PLATFORM_OK;
  return true;
}

bool platform_qspi_is_memory_mapped(void) {
  return qspi_memory_mapped ||
         ((QUADSPI->CCR & QUADSPI_CCR_FMODE) == QUADSPI_CCR_FMODE);
}
