#ifndef NAM_AUDIO_H
#define NAM_AUDIO_H

#include "platform.h"

#define AUDIO_SAMPLE_RATE_HZ 48000U
#define AUDIO_BLOCK_FRAMES 48U
#define AUDIO_CHANNELS 2U
#define AUDIO_REPORT_INTERVAL_MS 1000U

typedef struct {
  uint32_t rx_callbacks;
  uint32_t tx_callbacks;
  uint32_t overruns;
  uint32_t underruns;
  uint32_t dma_errors;
  uint32_t frame_errors;
  uint32_t buffer_phase_errors;
  uint32_t late_blocks;
  uint32_t worst_block_cycles;
} audio_stats_t;

platform_error_t audio_init(void);
platform_error_t audio_start(void);
void audio_get_stats(audio_stats_t *result);
void audio_report_configuration(void);
void audio_report_stats(void);

#endif
