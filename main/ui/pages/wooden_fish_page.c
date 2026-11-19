#include "wooden_fish_page.h"
#include "wooden_fish_art.h"
#include "ui/ui_theme.h"

#include <inttypes.h>

static lv_obj_t *s_count_label;
static uint32_t s_count;
static void (*s_on_strike)(void);

static void strike(lv_event_t *event)
{
    (void)event;
    if (s_on_strike) s_on_strike();
    if (s_count < UINT32_MAX) ++s_count;
    lv_label_set_text_fmt(s_count_label, "%" PRIu32, s_count);
}

lv_obj_t *wooden_fish_page_create(lv_obj_t *parent, void (*on_strike)(void))
{
    s_count = 0;
    s_on_strike = on_strike;
    lv_obj_t *page = ui_box(parent, 0, 44, 240, 216, 0x000000, 0);

    lv_obj_t *caption = lv_image_create(page);
    lv_image_set_src(caption, &wooden_fish_merit_caption);
    lv_obj_set_pos(caption, 14, 14);
    s_count_label = ui_text(page, "0", 56, 12, 172, &lv_font_montserrat_16, 0xFFFFFF);

    lv_obj_t *fish = lv_image_create(page);
    lv_image_set_src(fish, &wooden_fish_art);
    lv_obj_align(fish, LV_ALIGN_CENTER, 0, 12);
    lv_obj_add_flag(fish, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_image_opa(fish, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_add_event_cb(fish, strike, LV_EVENT_CLICKED, NULL);
    return page;
}
