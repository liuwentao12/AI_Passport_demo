#include "microphone.h"

#include <string.h>
#include "driver/i2s_std.h"
#include "esp_log.h"

static i2s_chan_handle_t s_rx;

esp_err_t microphone_begin(void)
{
    if (s_rx) return ESP_ERR_INVALID_STATE;
    const i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&channel, NULL, &s_rx);
    if (err != ESP_OK) return err;
    const i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = GPIO_NUM_26, .ws = GPIO_NUM_25,
                     .dout = I2S_GPIO_UNUSED, .din = GPIO_NUM_33},
    };
    err = i2s_channel_init_std_mode(s_rx, &config);
    if (err == ESP_OK) err = i2s_channel_enable(s_rx);
    if (err != ESP_OK) {
        i2s_del_channel(s_rx);
        s_rx = NULL;
    }
    return err;
}

esp_err_t microphone_read(int16_t *pcm, size_t capacity, size_t *samples)
{
    int32_t stereo[MIC_FRAME_SAMPLES * 2];
    size_t bytes = 0;
    *samples = 0;
    if (capacity > MIC_FRAME_SAMPLES) capacity = MIC_FRAME_SAMPLES;
    esp_err_t err = i2s_channel_read(s_rx, stereo, capacity * 2 * sizeof(int32_t), &bytes, 200);
    if (err != ESP_OK) return err;
    if (bytes == 0 || bytes % (2 * sizeof(int32_t))) return ESP_ERR_INVALID_SIZE;
    *samples = bytes / (2 * sizeof(int32_t));
    for (size_t i = 0; i < *samples; ++i) {
        /* INMP441 L/R is grounded. Convert the left slot and preserve diagnostic gain. */
        int32_t sample = (stereo[i * 2] >> 16) * 4;
        if (sample > INT16_MAX) sample = INT16_MAX;
        if (sample < INT16_MIN) sample = INT16_MIN;
        pcm[i] = (int16_t)sample;
    }
    return ESP_OK;
}

void microphone_end(void)
{
    esp_err_t err = i2s_channel_disable(s_rx);
    if (err == ESP_OK) err = i2s_del_channel(s_rx);
    if (err == ESP_OK) s_rx = NULL;
    else ESP_LOGE("microphone", "Close failed: %s", esp_err_to_name(err));
}

static void put_le(uint8_t *out, uint32_t number, unsigned bytes)
{
    for (unsigned i = 0; i < bytes; ++i) out[i] = (uint8_t)(number >> (i * 8));
}

void microphone_wav_header(uint8_t h[MIC_WAV_HEADER_SIZE], uint32_t pcm_bytes)
{
    memcpy(h, "RIFF", 4);
    put_le(h + 4, pcm_bytes + 36, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    put_le(h + 16, 16, 4);
    put_le(h + 20, 1, 2);
    put_le(h + 22, 1, 2);
    put_le(h + 24, MIC_SAMPLE_RATE, 4);
    put_le(h + 28, MIC_SAMPLE_RATE * 2, 4);
    put_le(h + 32, 2, 2);
    put_le(h + 34, 16, 2);
    memcpy(h + 36, "data", 4);
    put_le(h + 40, pcm_bytes, 4);
}
