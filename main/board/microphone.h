#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define MIC_SAMPLE_RATE 16000
#define MIC_FRAME_SAMPLES 256
#define MIC_WAV_HEADER_SIZE 44

/* One capture owner at a time. Read up to one frame of signed mono PCM. */
esp_err_t microphone_begin(void);
esp_err_t microphone_read(int16_t *pcm, size_t capacity, size_t *samples);
void microphone_end(void);
void microphone_wav_header(uint8_t header[MIC_WAV_HEADER_SIZE], uint32_t pcm_bytes);
