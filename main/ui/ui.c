#include "ui.h"
#include "ui_theme.h"
#include "pages/identity_page.h"
#include "pages/details_page.h"

static identity_page_t s_identity;
static details_page_t s_details;
static lv_obj_t *s_pages[UI_PAGE_COUNT];
static lv_obj_t *s_buttons[UI_PAGE_DETAILS];
static lv_obj_t *s_button_labels[UI_PAGE_DETAILS];
static lv_obj_t *s_details_button;
static ui_page_t s_page;

static void navigation(lv_event_t *event)
{
    ui_show_page((ui_page_t)(uintptr_t)lv_event_get_user_data(event));
}

static lv_obj_t *placeholder(lv_obj_t *parent, const char *title, const char *status)
{
    lv_obj_t *page = ui_box(parent, 12, 54, 216, 202, 0xFFFFFF, 14);
    ui_text(page, title, 16, 16, 184, &lv_font_montserrat_20, UI_COLOR_INK);
    ui_text(page, status, 16, 52, 184, &lv_font_montserrat_12, UI_COLOR_MUTED);
    return page;
}

void ui_init(const identity_t *identity, bool touch_available)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_t *header = ui_box(screen, 0, 0, 240, 44, UI_COLOR_INK, 0);
    ui_text(header, "AI PASSPORT", 16, 13, 184, &lv_font_montserrat_16, 0xFFFFFF);
    s_details_button = ui_box(header, 168, 4, 60, 36, UI_COLOR_SOFT, 6);
    lv_obj_t *details = ui_text(s_details_button, "Details", 0, 10, 60,
                               &lv_font_montserrat_12, UI_COLOR_INK);
    lv_obj_set_style_text_align(details, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(s_details_button, navigation, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)UI_PAGE_DETAILS);
    identity_page_create(&s_identity, screen);
    details_page_create(&s_details, screen);
    s_pages[UI_PAGE_IDENTITY] = s_identity.root;
    s_pages[UI_PAGE_WOODEN_FISH] = placeholder(screen, "Wooden Fish", "Coming soon");
    s_pages[UI_PAGE_AI_CHAT] = placeholder(screen, "AI Chat", "Voice input coming soon");
    s_pages[UI_PAGE_SETTINGS] = placeholder(screen, "Settings", "Coming soon");
    s_pages[UI_PAGE_DETAILS] = s_details.root;
    const char *names[] = {"Identity", "Muyu", "AI Chat", "Settings"};
    for (int i = 0; i < UI_PAGE_DETAILS; ++i) {
        s_buttons[i] = ui_box(screen, 8 + i * 57, 262, 53, 38, 0xFFFFFF, 8);
        lv_obj_add_flag(s_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(s_buttons[i], LV_OPA_70, LV_STATE_PRESSED);
        lv_obj_add_event_cb(s_buttons[i], navigation, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_button_labels[i] = ui_text(s_buttons[i], names[i], 0, 11, 53, &lv_font_montserrat_12, UI_COLOR_INK);
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
    for (int i = 0; i < UI_PAGE_COUNT; ++i) {
        if (i == (int)page) lv_obj_remove_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (page == UI_PAGE_IDENTITY) lv_obj_remove_flag(s_details_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_details_button, LV_OBJ_FLAG_HIDDEN);
    const ui_page_t tab = page == UI_PAGE_DETAILS ? UI_PAGE_IDENTITY : page;
    for (int i = 0; i < UI_PAGE_DETAILS; ++i) {
        const bool selected = i == (int)tab;
        lv_obj_set_style_bg_color(s_buttons[i], lv_color_hex(selected ? UI_COLOR_ACCENT : 0xFFFFFF), 0);
        lv_obj_set_style_text_color(s_button_labels[i], lv_color_hex(selected ? 0xFFFFFF : UI_COLOR_MUTED), 0);
    }
}

ui_page_t ui_current_page(void)
{
    return s_page;
}
