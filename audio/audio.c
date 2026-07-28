#include "audio.h"

#include <string.h>

#define AUDIO_BUFFER_BLOCKS 2U
#define AUDIO_WORDS_PER_FRAME AUDIO_CHANNELS
#define AUDIO_WORDS_PER_BLOCK (AUDIO_BLOCK_FRAMES * AUDIO_WORDS_PER_FRAME)
#define AUDIO_DMA_WORD_COUNT (AUDIO_BUFFER_BLOCKS * AUDIO_WORDS_PER_BLOCK)
#define AUDIO_PLL3P_TARGET_HZ 49152000U
#define AUDIO_PLL3P_TOLERANCE_HZ 32U
#define AUDIO_PLL3_FRACN 5269U
#define AUDIO_MCLK_DIVIDER 4U
#define AUDIO_BITS_PER_FRAME 64U
#define AUDIO_DIAG_GPIO GPIOE
#define AUDIO_DIAG_PIN GPIO_PIN_4
#define AUDIO_FRAME_ERROR_MASK                                                \
  (HAL_SAI_ERROR_AFSDET | HAL_SAI_ERROR_LFSDET | HAL_SAI_ERROR_CNREADY |     \
   HAL_SAI_ERROR_WCKCFG | HAL_SAI_ERROR_TIMEOUT)

#define AUDIO_DMA_BUFFER                                                     \
  __attribute__((section(".audio_dma_buffers"), aligned(32)))

static SAI_HandleTypeDef sai_tx;
static SAI_HandleTypeDef sai_rx;
static DMA_HandleTypeDef dma_tx;
static DMA_HandleTypeDef dma_rx;
static volatile uint32_t audio_rx_buffer[AUDIO_DMA_WORD_COUNT] AUDIO_DMA_BUFFER;
static volatile uint32_t audio_tx_buffer[AUDIO_DMA_WORD_COUNT] AUDIO_DMA_BUFFER;
static volatile audio_stats_t audio_stats;
static uint32_t audio_kernel_hz;
static uint32_t audio_mclk_hz;
static uint32_t audio_block_budget_cycles;
static bool audio_initialized;
static bool audio_started;
static bool audio_msp_error;
static bool sai3_common_initialized;

_Static_assert(AUDIO_DMA_WORD_COUNT <= UINT16_MAX,
               "HAL SAI DMA length must fit uint16_t");
_Static_assert((AUDIO_SAMPLE_RATE_HZ % AUDIO_BLOCK_FRAMES) == 0U,
               "audio block must span an integer number of sample periods");

static void diagnostic_gpio_init(void) {
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOE_CLK_ENABLE();
  gpio.Pin = AUDIO_DIAG_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(AUDIO_DIAG_GPIO, &gpio);
  AUDIO_DIAG_GPIO->BSRR = (uint32_t)AUDIO_DIAG_PIN << 16U;
}

static bool audio_clock_configure(void) {
  RCC_PeriphCLKInitTypeDef clock = {0};
  clock.PeriphClockSelection = RCC_PERIPHCLK_SAI23;
  clock.Sai23ClockSelection = RCC_SAI23CLKSOURCE_PLL3;
  clock.PLL3.PLL3M = 5U;
  clock.PLL3.PLL3N = 78U;
  clock.PLL3.PLL3P = 8U;
  clock.PLL3.PLL3Q = 8U;
  clock.PLL3.PLL3R = 8U;
  clock.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
  clock.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  clock.PLL3.PLL3FRACN = AUDIO_PLL3_FRACN;
  if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) {
    return false;
  }

  audio_kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI23);
  return audio_kernel_hz >=
             (AUDIO_PLL3P_TARGET_HZ - AUDIO_PLL3P_TOLERANCE_HZ) &&
         audio_kernel_hz <=
             (AUDIO_PLL3P_TARGET_HZ + AUDIO_PLL3P_TOLERANCE_HZ);
}

static void dma_defaults(DMA_HandleTypeDef *dma, DMA_Stream_TypeDef *instance,
                         uint32_t request, uint32_t direction) {
  memset(dma, 0, sizeof(*dma));
  dma->Instance = instance;
  dma->Init.Request = request;
  dma->Init.Direction = direction;
  dma->Init.PeriphInc = DMA_PINC_DISABLE;
  dma->Init.MemInc = DMA_MINC_ENABLE;
  dma->Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma->Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  dma->Init.Mode = DMA_CIRCULAR;
  dma->Init.Priority = DMA_PRIORITY_VERY_HIGH;
  dma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
}

void HAL_SAI_MspInit(SAI_HandleTypeDef *handle) {
  GPIO_InitTypeDef gpio = {0};

  if (handle->Instance != SAI3_Block_A &&
      handle->Instance != SAI3_Block_B) {
    return;
  }

  if (!sai3_common_initialized) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_SAI3_CLK_ENABLE();
    __HAL_RCC_SAI3_FORCE_RESET();
    __HAL_RCC_SAI3_RELEASE_RESET();
    HAL_NVIC_SetPriority(SAI3_IRQn, 4U, 0U);
    HAL_NVIC_EnableIRQ(SAI3_IRQn);
    sai3_common_initialized = true;
  }

  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF6_SAI3;

  if (handle->Instance == SAI3_Block_A) {
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio);

    dma_defaults(&dma_tx, DMA1_Stream1, DMA_REQUEST_SAI3_A,
                 DMA_MEMORY_TO_PERIPH);
    if (HAL_DMA_Init(&dma_tx) != HAL_OK) {
      audio_msp_error = true;
      return;
    }
    __HAL_LINKDMA(handle, hdmatx, dma_tx);
    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 4U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  } else {
    gpio.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOD, &gpio);

    dma_defaults(&dma_rx, DMA1_Stream0, DMA_REQUEST_SAI3_B,
                 DMA_PERIPH_TO_MEMORY);
    if (HAL_DMA_Init(&dma_rx) != HAL_OK) {
      audio_msp_error = true;
      return;
    }
    __HAL_LINKDMA(handle, hdmarx, dma_rx);
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 4U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  }
}

static void sai_common_defaults(SAI_HandleTypeDef *sai) {
  sai->Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  sai->Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
  sai->Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
  sai->Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  sai->Init.MonoStereoMode = SAI_STEREOMODE;
  sai->Init.CompandingMode = SAI_NOCOMPANDING;
  sai->Init.TriState = SAI_OUTPUT_NOTRELEASED;
  sai->Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;
  sai->Init.PdmInit.Activation = DISABLE;
}

static bool sai_blocks_configure(void) {
  memset(&sai_tx, 0, sizeof(sai_tx));
  memset(&sai_rx, 0, sizeof(sai_rx));

  sai_tx.Instance = SAI3_Block_A;
  sai_common_defaults(&sai_tx);
  sai_tx.Init.AudioMode = SAI_MODEMASTER_TX;
  sai_tx.Init.Synchro = SAI_ASYNCHRONOUS;
  sai_tx.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
  sai_tx.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
  sai_tx.Init.MckOutput = SAI_MCK_OUTPUT_ENABLE;
  if (HAL_SAI_InitProtocol(&sai_tx, SAI_I2S_STANDARD,
                           SAI_PROTOCOL_DATASIZE_24BIT,
                           AUDIO_CHANNELS) != HAL_OK) {
    return false;
  }

  sai_rx.Instance = SAI3_Block_B;
  sai_common_defaults(&sai_rx);
  sai_rx.Init.AudioMode = SAI_MODESLAVE_RX;
  sai_rx.Init.Synchro = SAI_SYNCHRONOUS;
  sai_rx.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_MCKDIV;
  sai_rx.Init.Mckdiv = 0U;
  sai_rx.Init.MckOutput = SAI_MCK_OUTPUT_DISABLE;
  if (HAL_SAI_InitProtocol(&sai_rx, SAI_I2S_STANDARD,
                           SAI_PROTOCOL_DATASIZE_24BIT,
                           AUDIO_CHANNELS) != HAL_OK) {
    return false;
  }

  if (sai_tx.Init.Mckdiv != AUDIO_MCLK_DIVIDER) {
    return false;
  }
  audio_mclk_hz = audio_kernel_hz / sai_tx.Init.Mckdiv;
  return true;
}

static bool tx_buffer_half_is_available(uint32_t offset) {
  uint32_t remaining = __HAL_DMA_GET_COUNTER(&dma_tx);
  if (offset == 0U) {
    return remaining > 0U && remaining <= AUDIO_WORDS_PER_BLOCK;
  }
  return remaining > AUDIO_WORDS_PER_BLOCK &&
         remaining <= AUDIO_DMA_WORD_COUNT;
}

static void process_audio_block(uint32_t offset) {
  uint32_t start_cycles;
  uint32_t elapsed_cycles;
  bool phase_ok_before;
  bool phase_ok_after;

  phase_ok_before = tx_buffer_half_is_available(offset);
  start_cycles = DWT->CYCCNT;
  AUDIO_DIAG_GPIO->BSRR = AUDIO_DIAG_PIN;
  for (uint32_t word = 0U; word < AUDIO_WORDS_PER_BLOCK; ++word) {
    audio_tx_buffer[offset + word] = audio_rx_buffer[offset + word];
  }
  __DMB();
  AUDIO_DIAG_GPIO->BSRR = (uint32_t)AUDIO_DIAG_PIN << 16U;
  elapsed_cycles = DWT->CYCCNT - start_cycles;
  phase_ok_after = tx_buffer_half_is_available(offset);

  ++audio_stats.rx_callbacks;
  if (!phase_ok_before || !phase_ok_after) {
    ++audio_stats.buffer_phase_errors;
  }
  if (elapsed_cycles > audio_stats.worst_block_cycles) {
    audio_stats.worst_block_cycles = elapsed_cycles;
  }
  if (elapsed_cycles >= audio_block_budget_cycles) {
    ++audio_stats.late_blocks;
  }
}

platform_error_t audio_init(void) {
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U) {
    return PLATFORM_ERROR_AUDIO_SAI;
  }

  memset((void *)audio_rx_buffer, 0, sizeof(audio_rx_buffer));
  memset((void *)audio_tx_buffer, 0, sizeof(audio_tx_buffer));
  memset((void *)&audio_stats, 0, sizeof(audio_stats));
  audio_msp_error = false;
  diagnostic_gpio_init();

  if (!audio_clock_configure()) {
    return PLATFORM_ERROR_AUDIO_CLOCK;
  }
  if (!sai_blocks_configure()) {
    return PLATFORM_ERROR_AUDIO_SAI;
  }
  if (audio_msp_error) {
    return PLATFORM_ERROR_AUDIO_DMA;
  }

  audio_block_budget_cycles =
      SystemCoreClock / (AUDIO_SAMPLE_RATE_HZ / AUDIO_BLOCK_FRAMES);
  audio_initialized = true;
  return PLATFORM_OK;
}

platform_error_t audio_start(void) {
  if (!audio_initialized || audio_started) {
    return PLATFORM_ERROR_AUDIO_SAI;
  }

  if (HAL_SAI_Receive_DMA(
          &sai_rx, (uint8_t *)(uintptr_t)audio_rx_buffer,
          (uint16_t)AUDIO_DMA_WORD_COUNT) != HAL_OK) {
    return PLATFORM_ERROR_AUDIO_DMA;
  }
  if (HAL_SAI_Transmit_DMA(
          &sai_tx, (uint8_t *)(uintptr_t)audio_tx_buffer,
          (uint16_t)AUDIO_DMA_WORD_COUNT) != HAL_OK) {
    (void)HAL_SAI_DMAStop(&sai_rx);
    return PLATFORM_ERROR_AUDIO_DMA;
  }

  audio_started = true;
  return PLATFORM_OK;
}

void audio_get_stats(audio_stats_t *result) {
  uint32_t primask;
  if (result == NULL) {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  *result = audio_stats;
  if (primask == 0U) {
    __enable_irq();
  }
}

void audio_report_configuration(void) {
  platform_uart_write("audio.mode=passthrough\r\n");
  platform_uart_write("audio.format=i2s-24-in-32-stereo\r\n");
  platform_uart_write_dec("audio.sample_rate.hz=", AUDIO_SAMPLE_RATE_HZ);
  platform_uart_write_dec("audio.block.frames=", AUDIO_BLOCK_FRAMES);
  platform_uart_write_dec("audio.block.budget.cycles=",
                          audio_block_budget_cycles);
  platform_uart_write_dec("audio.sai.kernel.hz=", audio_kernel_hz);
  platform_uart_write_dec("audio.mclk.hz=", audio_mclk_hz);
  platform_uart_write_dec("audio.bclk.hz=",
                          AUDIO_SAMPLE_RATE_HZ * AUDIO_BITS_PER_FRAME);
  platform_uart_write_dec("audio.dma.buffer.bytes=",
                          sizeof(audio_rx_buffer) + sizeof(audio_tx_buffer));
  platform_uart_write("audio.diag.gpio=PE4\r\n");
  platform_uart_write("audio.pcm1808.straps=MD1:0,MD0:0,FMT:0\r\n");
  platform_uart_write("audio.pcm5102.straps=SCK:GND,FMT:0\r\n");
}

void audio_report_stats(void) {
  audio_stats_t stats;
  uint32_t cycles_per_us = SystemCoreClock / 1000000U;
  uint32_t xruns;

  audio_get_stats(&stats);
  xruns = stats.overruns + stats.underruns + stats.dma_errors +
          stats.frame_errors + stats.buffer_phase_errors + stats.late_blocks;
  platform_uart_write_dec("audio.rx.callbacks=", stats.rx_callbacks);
  platform_uart_write_dec("audio.tx.callbacks=", stats.tx_callbacks);
  platform_uart_write_dec("audio.overruns=", stats.overruns);
  platform_uart_write_dec("audio.underruns=", stats.underruns);
  platform_uart_write_dec("audio.dma.errors=", stats.dma_errors);
  platform_uart_write_dec("audio.frame.errors=", stats.frame_errors);
  platform_uart_write_dec("audio.buffer.phase.errors=",
                          stats.buffer_phase_errors);
  platform_uart_write_dec("audio.late.blocks=", stats.late_blocks);
  platform_uart_write_dec("audio.xruns=", xruns);
  platform_uart_write_dec("audio.worst.cycles=", stats.worst_block_cycles);
  platform_uart_write_dec(
      "audio.worst.us=",
      (stats.worst_block_cycles + cycles_per_us - 1U) / cycles_per_us);
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *handle) {
  if (handle->Instance == SAI3_Block_B) {
    process_audio_block(0U);
  }
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *handle) {
  if (handle->Instance == SAI3_Block_B) {
    process_audio_block(AUDIO_WORDS_PER_BLOCK);
  }
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *handle) {
  if (handle->Instance == SAI3_Block_A) {
    ++audio_stats.tx_callbacks;
  }
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *handle) {
  if (handle->Instance == SAI3_Block_A) {
    ++audio_stats.tx_callbacks;
  }
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *handle) {
  uint32_t errors = handle->ErrorCode;
  if ((errors & HAL_SAI_ERROR_OVR) != 0U) {
    ++audio_stats.overruns;
  }
  if ((errors & HAL_SAI_ERROR_UDR) != 0U) {
    ++audio_stats.underruns;
  }
  if ((errors & HAL_SAI_ERROR_DMA) != 0U) {
    ++audio_stats.dma_errors;
  }
  if ((errors & AUDIO_FRAME_ERROR_MASK) != 0U) {
    ++audio_stats.frame_errors;
  }
  handle->ErrorCode = HAL_SAI_ERROR_NONE;
}

void DMA1_Stream0_IRQHandler(void) {
  HAL_DMA_IRQHandler(&dma_rx);
}

void DMA1_Stream1_IRQHandler(void) {
  HAL_DMA_IRQHandler(&dma_tx);
}

void SAI3_IRQHandler(void) {
  HAL_SAI_IRQHandler(&sai_tx);
  HAL_SAI_IRQHandler(&sai_rx);
}
