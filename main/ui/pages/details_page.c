#include "details_page.h"
#include "ui/ui_theme.h"

static lv_obj_t *full_text(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = ui_text(parent, "", 0, 0, 184, font, color);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    return label;
}

void details_page_create(details_page_t *page, lv_obj_t *parent)
{
    page->root = ui_box(parent, 12, 54, 216, 202, 0xFFFFFF, 14);
    lv_obj_add_flag(page->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(page->root, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page->root, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(page->root, 16, 0);
    lv_obj_set_style_pad_row(page->root, 8, 0);
    lv_obj_set_style_bg_color(page->root, lv_color_hex(UI_COLOR_MUTED), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(page->root, LV_OPA_50, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(page->root, 3, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(page->root, LV_FLEX_FLOW_COLUMN);
    ui_text(page->root, "PROFILE / DEMO", 0, 0, 184, &lv_font_montserrat_12, UI_COLOR_ACCENT);
    page->name = full_text(page->root, &lv_font_montserrat_20, UI_COLOR_INK);
    page->role = full_text(page->root, &lv_font_montserrat_12, UI_COLOR_MUTED);
    ui_text(page->root, "PASSPORT NUMBER", 0, 0, 184, &lv_font_montserrat_12, UI_COLOR_ACCENT);
    page->number = full_text(page->root, &lv_font_montserrat_14, UI_COLOR_INK);
    page->location = full_text(page->root, &lv_font_montserrat_14, UI_COLOR_MUTED);
    ui_text(page->root, "ISSUED", 0, 0, 184, &lv_font_montserrat_12, UI_COLOR_ACCENT);
    page->issued = full_text(page->root, &lv_font_montserrat_14, UI_COLOR_INK);
    page->description = full_text(page->root, &lv_font_montserrat_14, UI_COLOR_INK);
}

void details_page_update(details_page_t *page, const identity_t *data)
{
    lv_label_set_text(page->name, data->name);
    lv_label_set_text(page->role, data->role);
    lv_label_set_text(page->number, data->number);
    lv_label_set_text(page->location, data->location);
    lv_label_set_text(page->issued, data->issued);
    lv_label_set_text(page->description, data->description);
}
