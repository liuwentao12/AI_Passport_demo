#pragma once

#include "lvgl.h"

#define UI_COLOR_BG       0xEDF2F2
#define UI_COLOR_INK      0x142D35
#define UI_COLOR_MUTED    0x61777D
#define UI_COLOR_ACCENT   0x087F75
#define UI_COLOR_SOFT     0xDDF2EC
#define UI_COLOR_LINE     0xDCE6E5

/* Shared primitives keep spacing and fonts consistent across pages. */
lv_obj_t *ui_box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius);
lv_obj_t *ui_text(lv_obj_t *parent, const char *text, int x, int y, int width,
                  const lv_font_t *font, uint32_t color);
