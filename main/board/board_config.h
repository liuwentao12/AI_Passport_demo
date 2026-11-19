#pragma once

#include <stdbool.h>
#include "driver/spi_master.h"

/* Keep these aligned with the wiring validated by touchscreen_test.c. */
#define BOARD_LCD_HOST          SPI2_HOST
#define BOARD_LCD_SCK           18
#define BOARD_LCD_MOSI          23
#define BOARD_LCD_MISO          19
#define BOARD_LCD_CS            16
#define BOARD_LCD_DC            21
#define BOARD_LCD_RST           22
#define BOARD_TOUCH_CS          17
#define BOARD_LCD_WIDTH         240
#define BOARD_LCD_HEIGHT        320
#define BOARD_LCD_PIXEL_CLOCK   (10 * 1000 * 1000)
#define BOARD_LCD_BUFFER_ROWS   20

/* Panel scan direction: compensate for the reported left/right mirrored text.
 * These are controller settings; LVGL still uses a 240 x 320 portrait canvas. */
#define BOARD_LCD_MIRROR_X      true
#define BOARD_LCD_MIRROR_Y      false
