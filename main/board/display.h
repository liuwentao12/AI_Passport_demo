#pragma once

#include <stdint.h>
#include "esp_err.h"

/* Initialize once. The application treats initialization errors as fatal.
 * The display and SPI bus live until reset; only the UI task may draw. */
esp_err_t board_display_init(void);

/* Coordinates are end-exclusive. Buffer is DMA-capable, wire-order RGB565.
 * Blocks until DMA completes: the caller can safely reuse pixels on return. */
esp_err_t board_display_draw(int x1, int y1, int x2, int y2, const uint8_t *pixels);
