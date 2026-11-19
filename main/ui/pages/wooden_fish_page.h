#pragma once

#include "lvgl.h"

/* Called once in the UI task. on_strike may be NULL for a silent page. */
lv_obj_t *wooden_fish_page_create(lv_obj_t *parent, void (*on_strike)(void));
