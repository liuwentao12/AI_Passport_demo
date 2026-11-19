#pragma once

#include "lvgl.h"
#include "app/voice_chat.h"

lv_obj_t *voice_chat_page_create(lv_obj_t *parent);
void voice_chat_page_update(const voice_chat_update_t *update);
