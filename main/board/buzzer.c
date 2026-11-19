#include "buzzer.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "buzzer";
static bool s_ready;
static int64_t s_stop_us;

esp_err_t board_buzzer_init(void)
{
    if (!GPIO_IS_VALID_OUTPUT_GPIO(BOARD_BUZZER_GPIO)) return ESP_ERR_INVALID_ARG;
    if (BOARD_BUZZER_ACTIVE) {
        const gpio_config_t config = {
            .pin_bit_mask = UINT64_C(1) << BOARD_BUZZER_GPIO,
            .mode = GPIO_MODE_OUTPUT,
        };
        esp_err_t err = gpio_set_level(BOARD_BUZZER_GPIO, BOARD_BUZZER_ACTIVE_LOW);
        if (err != ESP_OK) return err;
        err = gpio_config(&config);
        s_ready = err == ESP_OK;
        return err;
    }

    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) return err;
    const ledc_channel_config_t channel = {
        .gpio_num = BOARD_BUZZER_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .flags.output_invert = BOARD_BUZZER_ACTIVE_LOW,
    };
    err = ledc_channel_config(&channel);
    s_ready = err == ESP_OK;
    return err;
}

static esp_err_t set_sound(bool on)
{
    if (BOARD_BUZZER_ACTIVE) return gpio_set_level(BOARD_BUZZER_GPIO, on != BOARD_BUZZER_ACTIVE_LOW);
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, on ? 128 : 0);
    if (err != ESP_OK) return err;
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void board_buzzer_strike(void)
{
    if (!s_ready) return;
    esp_err_t err = set_sound(true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Start failed: %s", esp_err_to_name(err));
    }
    s_stop_us = esp_timer_get_time() + 60000;
}

void board_buzzer_poll(void)
{
    if (s_stop_us == 0 || esp_timer_get_time() < s_stop_us) return;
    esp_err_t err = set_sound(false);
    if (err == ESP_OK) {
        s_stop_us = 0;
    } else {
        ESP_LOGW(TAG, "Stop failed: %s", esp_err_to_name(err));
        s_stop_us = esp_timer_get_time() + 60000;
    }
}
