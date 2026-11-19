#include "voice_chat_page.h"
#include "ui/ui_theme.h"

static lv_obj_t *s_status;
static lv_obj_t *s_text;
static lv_obj_t *s_scroll;

lv_obj_t *voice_chat_page_create(lv_obj_t *parent)
{
    lv_obj_t *page = ui_box(parent, 12, 54, 216, 202, 0xFFFFFF, 14);
    ui_text(page, "AI Voice", 12, 10, 192, &lv_font_montserrat_20, UI_COLOR_INK);
    s_status = ui_text(page, "KEY3 to record", 12, 38, 192, &lv_font_montserrat_12, UI_COLOR_ACCENT);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s_status, 30);

    s_scroll = ui_box(page, 10, 72, 196, 120, UI_COLOR_BG, 8);
    lv_obj_add_flag(s_scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_scroll, LV_DIR_VER);
    lv_obj_set_style_pad_all(s_scroll, 8, 0);
    const lv_font_t *font = &lv_font_montserrat_14;
#if LV_FONT_SOURCE_HAN_SANS_SC_14_CJK
    font = &lv_font_source_han_sans_sc_14_cjk;
#endif
    s_text = ui_text(s_scroll, "Ask a short question.", 0, 0, 180, font, UI_COLOR_INK);
    lv_label_set_long_mode(s_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s_text, LV_SIZE_CONTENT);
    return page;
}

void voice_chat_page_update(const voice_chat_update_t *update)
{
    lv_label_set_text_static(s_status, update->status);
    if (update->answer[0]) {
        lv_label_set_text_fmt(s_text, "Q: %s\n\nAI: %s", update->question, update->answer);
    } else if (update->question[0]) {
        lv_label_set_text_fmt(s_text, "Q: %s", update->question);
    } else {
        lv_label_set_text(s_text, "");
    }
    lv_obj_scroll_to_y(s_scroll, 0, LV_ANIM_OFF);
}
