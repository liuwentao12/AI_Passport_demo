#include "idle_lock.h"
#include "ui_theme.h"

#define IDLE_TIMEOUT_MS 60000U

static lv_display_t *s_display;
static lv_obj_t *s_overlay;
static void (*s_set_dimmed)(bool);

static void center_text(const char *text, int32_t y, int32_t width,
                        const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = ui_text(s_overlay, text, 0, 0, width, font, color);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, y);
}

void idle_lock_start(void (*set_dimmed)(bool))
{
    s_display = lv_display_get_default();
    s_set_dimmed = set_dimmed;
    lv_display_trigger_activity(s_display);
}

void idle_lock_poll(void)
{
    if (!s_display || s_overlay || lv_display_get_inactive_time(s_display) < IDLE_TIMEOUT_MS) return;
    const int32_t w = lv_display_get_horizontal_resolution(s_display);
    const int32_t h = lv_display_get_vertical_resolution(s_display);
    s_overlay = ui_box(lv_display_get_layer_top(s_display), 0, 0, w, h, 0x091519, 0);
    center_text("AI PASSPORT", -34, w - 24, &lv_font_montserrat_12, 0x5B8894);
    center_text("SCREEN LOCKED", -8, w - 24, &lv_font_montserrat_20, 0x91BCC6);
    ui_box(s_overlay, (w - 36) / 2, h / 2 + 14, 36, 1, 0x426874, 0);
    center_text("Touch or press a key to wake", 36, w - 24, &lv_font_montserrat_12, 0x719BA6);

    /* Send one static frame, then leave LCD GRAM displaying it while input
     * polling and background UI updates continue. */
    lv_refr_now(s_display);
    lv_display_enable_invalidation(s_display, false);
    lv_timer_pause(lv_display_get_refr_timer(s_display));
    if (s_set_dimmed) s_set_dimmed(true);
}

bool idle_lock_handle_input(void)
{
    if (!s_display) return false;
    lv_display_trigger_activity(s_display);
    if (!s_overlay) return false;

    lv_display_enable_invalidation(s_display, true);
    lv_obj_delete(s_overlay);
    s_overlay = NULL;
    /* Include changes made to the underlying page while invalidation was paused. */
    lv_obj_invalidate(lv_display_get_screen_active(s_display));
    lv_timer_ready(lv_display_get_refr_timer(s_display));
    if (s_set_dimmed) s_set_dimmed(false);
    return true;
}
