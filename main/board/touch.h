#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} board_touch_sample_t;

/* Implement the controller here once its model is known. Read in the UI task.
 * Return calibrated portrait pixel coordinates, x=0..239 and y=0..319.
 * Reuse BOARD_LCD_HOST; do not initialize the shared SPI bus a second time. */
bool board_touch_is_available(void);
bool board_touch_read(board_touch_sample_t *sample);
