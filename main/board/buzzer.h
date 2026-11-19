#pragma once

#include "esp_err.h"

esp_err_t board_buzzer_init(void);
/* Call these from the UI task. A new strike extends the current pulse. */
void board_buzzer_strike(void);
void board_buzzer_poll(void);
