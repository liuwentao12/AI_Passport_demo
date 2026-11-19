#include "identity_page.h"
#include "ui/ui_theme.h"

void identity_page_create(identity_page_t *page, lv_obj_t *parent)
{
    page->root = ui_box(parent, 12, 54, 216, 202, 0xFFFFFF, 14);
    ui_text(page->root, "PERSONAL ID", 16, 14, 132, &lv_font_montserrat_12, UI_COLOR_MUTED);
    lv_obj_t *badge = ui_box(page->root, 157, 10, 47, 22, UI_COLOR_SOFT, 6);
    lv_obj_t *demo = ui_text(badge, "DEMO", 0, 4, 47, &lv_font_montserrat_12, UI_COLOR_ACCENT);
    lv_obj_set_style_text_align(demo, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *avatar = ui_box(page->root, 16, 48, 48, 48, UI_COLOR_SOFT, LV_RADIUS_CIRCLE);
    page->initials = ui_text(avatar, "", 0, 12, 48, &lv_font_montserrat_20, UI_COLOR_ACCENT);
    lv_obj_set_style_text_align(page->initials, LV_TEXT_ALIGN_CENTER, 0);
    page->name = ui_text(page->root, "", 76, 50, 124, &lv_font_montserrat_20, UI_COLOR_INK);
    page->role = ui_text(page->root, "", 76, 79, 124, &lv_font_montserrat_12, UI_COLOR_MUTED);
    ui_box(page->root, 16, 111, 184, 1, UI_COLOR_LINE, 0);
    ui_text(page->root, "PASSPORT NUMBER", 16, 126, 184, &lv_font_montserrat_12, UI_COLOR_MUTED);
    page->number = ui_text(page->root, "", 16, 146, 184, &lv_font_montserrat_16, UI_COLOR_INK);
    ui_box(page->root, 16, 179, 6, 6, UI_COLOR_ACCENT, LV_RADIUS_CIRCLE);
    page->location = ui_text(page->root, "", 29, 175, 171, &lv_font_montserrat_12, UI_COLOR_MUTED);
}

void identity_page_update(identity_page_t *page, const identity_t *data)
{
    lv_label_set_text(page->name, data->name);
    lv_label_set_text(page->initials, data->initials);
    lv_label_set_text(page->role, data->role);
    lv_label_set_text(page->number, data->number);
    lv_label_set_text(page->location, data->location);
}
