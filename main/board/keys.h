#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef enum { BOARD_KEY1, BOARD_KEY2, BOARD_KEY3, BOARD_KEY4, BOARD_KEY_COUNT } board_key_t;

esp_err_t board_keys_init(void);

/* Call from the UI task. Each set bit represents one debounced press. */
uint32_t board_keys_poll(void);
