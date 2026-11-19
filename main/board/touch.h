#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "touch_transform.h"

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} board_touch_sample_t;

/* UI task only. Initialize after board_display_init() owns the shared bus. */
esp_err_t board_touch_init(void);
/* Available means initialized AND calibrated. */
bool board_touch_is_available(void);
/* true = successful sample (pressed may be false); false = unavailable/I/O error. */
bool board_touch_read(board_touch_sample_t *sample);
bool board_touch_read_raw(board_touch_sample_t *sample);
/* Fit/validate before calling; activates in RAM and tries to persist to NVS.
 * A persistence error leaves the current calibration usable until reboot. */
esp_err_t board_touch_set_calibration(const touch_point_t raw[3]);
const touch_point_t *board_touch_calibration_targets(void);
