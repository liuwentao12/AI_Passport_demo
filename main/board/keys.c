#include "keys.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const int s_pins[BOARD_KEY_COUNT] = {
    BOARD_KEY1_GPIO, BOARD_KEY2_GPIO, BOARD_KEY3_GPIO, BOARD_KEY4_GPIO,
};

static struct {
    bool raw_pressed;
    bool stable_pressed;
    int64_t changed_us;
} s_keys[BOARD_KEY_COUNT];

esp_err_t board_keys_init(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < BOARD_KEY_COUNT; ++i) {
        int pin = s_pins[i];
        if (pin == -1) continue;
#if CONFIG_IDF_TARGET_ESP32
        if ((pin >= 6 && pin <= 11) || pin >= 34) return ESP_ERR_INVALID_ARG;
#endif
        if (pin < 0 || pin >= 64 || !GPIO_IS_VALID_GPIO(pin)) return ESP_ERR_INVALID_ARG;
        uint64_t bit = UINT64_C(1) << pin;
        if (mask & bit) return ESP_ERR_INVALID_ARG;
        mask |= bit;
    }
    if (!mask) return ESP_OK;
    const gpio_config_t config = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&config);
}

uint32_t board_keys_poll(void)
{
    uint32_t presses = 0;
    const int64_t now = esp_timer_get_time();
    for (int i = 0; i < BOARD_KEY_COUNT; ++i) {
        if (s_pins[i] == -1) continue;
        const bool pressed = gpio_get_level((gpio_num_t)s_pins[i]) == 0;
        if (pressed != s_keys[i].raw_pressed) {
            s_keys[i].raw_pressed = pressed;
            s_keys[i].changed_us = now;
        } else if (pressed != s_keys[i].stable_pressed && now - s_keys[i].changed_us >= 30000) {
            s_keys[i].stable_pressed = pressed;
            if (pressed) presses |= UINT32_C(1) << i;
        }
    }
    return presses;
}
