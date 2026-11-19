#pragma once

#include "app/identity.h"

typedef enum {
    UI_PAGE_IDENTITY,
    UI_PAGE_WOODEN_FISH,
    UI_PAGE_AI_CHAT,
    UI_PAGE_SETTINGS,
    UI_PAGE_DETAILS,
    UI_PAGE_COUNT,
} ui_page_t;

/* All calls must originate from the single UI task. Create once, update many.
 * LVGL and the display must already be initialized. */
void ui_init(const identity_t *identity, bool touch_available);
void ui_set_identity(const identity_t *identity);
void ui_show_page(ui_page_t page);
ui_page_t ui_current_page(void);
