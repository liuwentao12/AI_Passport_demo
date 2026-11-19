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
#define BOARD_LCD_BACKLIGHT_GPIO -1   /* Optional PWM-capable backlight control pin. */
#define BOARD_LCD_BACKLIGHT_ACTIVE_LOW false

/* Panel scan direction: compensate for the reported left/right mirrored text.
 * These are controller settings; LVGL still uses a 240 x 320 portrait canvas. */
#define BOARD_LCD_MIRROR_X      true
#define BOARD_LCD_MIRROR_Y      false

/* XPT2046 shares SPI2 with the LCD. PEN is unconnected: poll Z1/Z2. */
#define BOARD_TOUCH_CLOCK_HZ    (1000 * 1000)
#define BOARD_TOUCH_PRESSURE    400
/* Set true for one boot to replace the saved calibration, then restore false. */
#define BOARD_TOUCH_FORCE_CALIBRATION false

/* Active-low switches to GND. -1 leaves the input disabled. On the classic
 * ESP32, GPIO9/10/11 are Flash signals and must not be used for the keys. */
#define BOARD_KEY1_GPIO         27
#define BOARD_KEY2_GPIO         32
#define BOARD_KEY3_GPIO         14
#define BOARD_KEY4_GPIO         13

/* GPIO35 is input-only on ESP32. Connect the buzzer signal to GPIO4. */
#define BOARD_BUZZER_GPIO       4
#define BOARD_BUZZER_ACTIVE     false /* true for a self-oscillating module */
#define BOARD_BUZZER_ACTIVE_LOW false

/* PN532 HSU, 3.3 V logic. Cross UART TX and RX. */
#define BOARD_NFC_UART          UART_NUM_2
#define BOARD_NFC_TX_GPIO       5
#define BOARD_NFC_RX_GPIO       34
