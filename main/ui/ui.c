#include "ui.h"
#include "ui_theme.h"
#include "pages/identity_page.h"
#include "pages/details_page.h"

static identity_page_t s_identity;
static details_page_t s_details;
static lv_obj_t *s_buttons[UI_PAGE_COUNT];
static lv_obj_t *s_button_labels[UI_PAGE_COUNT];
static ui_page_t s_page;

static void navigation(lv_event_t *event)
{
    ui_show_page((ui_page_t)(uintptr_t)lv_event_get_user_data(event));
}

void ui_init(const identity_t *identity, bool touch_available)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_t *header = ui_box(screen, 0, 0, 240, 44, UI_COLOR_INK, 0);
    ui_text(header, "AI PASSPORT", 16, 13, 184, &lv_font_montserrat_16, 0xFFFFFF);
    ui_box(header, 216, 19, 7, 7, 0x54CBB2, LV_RADIUS_CIRCLE);
    identity_page_create(&s_identity, screen);
    details_page_create(&s_details, screen);
    const char *names[] = {"Identity", "Details"};
    for (int i = 0; i < UI_PAGE_COUNT; ++i) {
        s_buttons[i] = ui_box(screen, 12 + i * 112, 268, 104, 32, 0xFFFFFF, 8);
        lv_obj_add_flag(s_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(s_buttons[i], LV_OPA_70, LV_STATE_PRESSED);
        lv_obj_add_event_cb(s_buttons[i], navigation, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_button_labels[i] = ui_text(s_buttons[i], names[i], 0, 7, 104, &lv_font_montserrat_14, UI_COLOR_INK);
        lv_obj_set_style_text_align(s_button_labels[i], LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_t *hint = ui_text(screen, touch_available ? "LOCAL DEMO PROFILE" : "TOUCH NOT CONFIGURED",
                             12, 305, 216, &lv_font_montserrat_12, UI_COLOR_MUTED);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    ui_set_identity(identity);
    ui_show_page(UI_PAGE_IDENTITY);
}

void ui_set_identity(const identity_t *identity)
{
    if (!identity_is_valid(identity)) return;
    identity_page_update(&s_identity, identity);
    details_page_update(&s_details, identity);
}

void ui_show_page(ui_page_t page)
{
    if ((unsigned)page >= UI_PAGE_COUNT) return;
    s_page = page;
    lv_obj_add_flag(s_identity.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_details.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(page == UI_PAGE_IDENTITY ? s_identity.root : s_details.root, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < UI_PAGE_COUNT; ++i) {
        lv_obj_set_style_bg_color(s_buttons[i], lv_color_hex(i == (int)page ? UI_COLOR_ACCENT : 0xFFFFFF), 0);
        lv_obj_set_style_text_color(s_button_labels[i], lv_color_hex(i == (int)page ? 0xFFFFFF : UI_COLOR_MUTED), 0);
    }
}

ui_page_t ui_current_page(void)
{
    return s_page;
}
