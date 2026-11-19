#include "boot_screen.h"
#include "lvgl.h"

enum {
    READY_MS = 2240,
    FADE_MS = 2660,
    BOOT_MS = 3000,
};

static const uint32_t ice = 0xBDF5FF;
static const uint32_t cyan = 0x29E9FF;
static const uint32_t muted = 0x72ACBD;

static struct {
    lv_obj_t *root;
    lv_obj_t *outer;
    lv_obj_t *inner;
    lv_obj_t *scan;
    lv_obj_t *brand;
    lv_obj_t *subtitle;
    lv_obj_t *bar;
    lv_obj_t *percent;
    lv_obj_t *status;
    lv_obj_t *note;
    const char *(*prepare_home)(void);
    int32_t height;
    int32_t progress;
    bool ready;
} boot;

static void draw_strip(lv_layer_t *layer, const lv_draw_rect_dsc_t *style,
                       int32_t x, int32_t y, int32_t w, int32_t h)
{
    const lv_area_t area = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, style, &area);
}

static void draw_grid(lv_event_t *event)
{
    lv_obj_t *root = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(root, &area);
    const int32_t w = lv_area_get_width(&area);
    const int32_t h = lv_area_get_height(&area);
    const lv_opa_t opacity = lv_obj_get_style_opa_recursive(root, LV_PART_MAIN);
    lv_draw_rect_dsc_t style;
    lv_draw_rect_dsc_init(&style);
    style.bg_opa = opacity;
    style.bg_color = lv_color_hex(0x09202B);

    for (int32_t x = 12; x < w - 12; x += 16) {
        draw_strip(layer, &style, area.x1 + x, area.y1 + 12, 1, h - 24);
    }
    for (int32_t y = 12; y < h - 12; y += 16) {
        draw_strip(layer, &style, area.x1 + 12, area.y1 + y, w - 24, 1);
    }

    style.bg_color = lv_color_hex(0x2496AC);
    for (int i = 0; i < 4; ++i) {
        const int32_t x = area.x1 + ((i & 1) ? w - 10 : 8);
        const int32_t y = area.y1 + ((i & 2) ? h - 10 : 8);
        draw_strip(layer, &style, x - ((i & 1) ? 16 : 0), y, 18, 2);
        draw_strip(layer, &style, x, y - ((i & 2) ? 16 : 0), 2, 18);
    }
}

static lv_obj_t *make_label(const char *text, const lv_font_t *font,
                            uint32_t color, int32_t y)
{
    lv_obj_t *label = lv_label_create(boot.root);
    lv_label_set_text_static(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static lv_obj_t *make_arc(int32_t size, int32_t center, int32_t start, int32_t end,
                          uint32_t color)
{
    lv_obj_t *arc = lv_arc_create(boot.root);
    lv_obj_remove_style_all(arc);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(arc, size, size);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, center - size / 2);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_angles(arc, start, end);
    lv_obj_set_style_arc_width(arc, 1, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x153744), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    return arc;
}

static void set_progress(int32_t value)
{
    if (boot.progress == value) return;
    boot.progress = value;
    lv_bar_set_value(boot.bar, value, LV_ANIM_OFF);
    lv_label_set_text_fmt(boot.percent, "%ld%%", (long)value);
}

static void run_frame(void *root, int32_t elapsed)
{
    if (elapsed >= FADE_MS) {
        lv_obj_set_style_opa(root, (BOOT_MS - elapsed) * LV_OPA_COVER / (BOOT_MS - FADE_MS), 0);
    }

    if (!boot.ready && elapsed >= READY_MS) {
        const char *note = boot.prepare_home();
        boot.ready = true;
        lv_label_set_text_static(boot.status, "SYSTEM READY");
        lv_obj_set_style_text_color(boot.status, lv_color_hex(ice), 0);
        lv_label_set_text_static(boot.note, note ? note : "");
        set_progress(100);
    }
    if (elapsed >= FADE_MS) return;

    lv_arc_set_rotation(boot.outer, elapsed * 360 / 1650 % 360);
    lv_arc_set_rotation(boot.inner, 360 - elapsed * 360 / 1150 % 360);
    const int32_t span = boot.height - 24;
    const int32_t scan = elapsed * span / 1350 % (2 * span);
    lv_obj_set_y(boot.scan, 12 + (scan < span ? scan : 2 * span - scan));
    lv_obj_set_style_text_opa(boot.brand, LV_CLAMP(0, (elapsed - 240) * 255 / 480, 255), 0);
    lv_obj_set_style_text_opa(boot.subtitle, LV_CLAMP(0, (elapsed - 420) * 255 / 480, 255), 0);
    if (!boot.ready) set_progress(elapsed * 95 / READY_MS);
}

static void finish(lv_anim_t *anim)
{
    lv_obj_delete(anim->var);
    boot.root = NULL;
}

bool boot_screen_start(const char *(*prepare_home)(void))
{
    if (boot.root || !prepare_home) return false;
    lv_display_t *display = lv_display_get_default();
    const int32_t w = lv_display_get_horizontal_resolution(display);
    const int32_t h = lv_display_get_vertical_resolution(display);
    const int32_t center = h * 37 / 100;
    const int32_t diameter = LV_MIN(w, h) * 38 / 100;
    boot.height = h;
    boot.prepare_home = prepare_home;
    boot.progress = 0;
    boot.ready = false;
    boot.root = lv_obj_create(lv_layer_top());
    if (!boot.root) return false;
    lv_obj_remove_style_all(boot.root);
    lv_obj_remove_flag(boot.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(boot.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(boot.root, w, h);
    lv_obj_set_style_bg_color(boot.root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(boot.root, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(boot.root, draw_grid, LV_EVENT_DRAW_MAIN_END, NULL);

    boot.scan = lv_obj_create(boot.root);
    lv_obj_remove_style_all(boot.scan);
    lv_obj_remove_flag(boot.scan, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(boot.scan, w - 24, 2);
    lv_obj_set_x(boot.scan, 12);
    lv_obj_set_style_bg_color(boot.scan, lv_color_hex(cyan), 0);
    lv_obj_set_style_bg_opa(boot.scan, LV_OPA_20, 0);

    boot.outer = make_arc(diameter, center, 12, 126, cyan);
    boot.inner = make_arc(diameter - 18, center, 165, 284, ice);
    lv_obj_t *logo = make_label("AI", &lv_font_montserrat_20, ice, center);
    lv_obj_set_style_text_letter_space(logo, 3, 0);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, center - lv_font_get_line_height(&lv_font_montserrat_20) / 2);

    boot.brand = make_label("AI PASSPORT", &lv_font_montserrat_20, ice, h * 60 / 100);
    lv_obj_set_style_text_letter_space(boot.brand, 3, 0);
    boot.subtitle = make_label("PERSONAL INTELLIGENCE TERMINAL", &lv_font_montserrat_12,
                               muted, h * 60 / 100 + 25);
    boot.percent = make_label("0%", &lv_font_montserrat_12, cyan, h - 68);
    lv_obj_align(boot.percent, LV_ALIGN_TOP_RIGHT, -18, h - 68);
    boot.bar = lv_bar_create(boot.root);
    lv_obj_remove_style_all(boot.bar);
    lv_obj_set_size(boot.bar, w - 36, 4);
    lv_obj_align(boot.bar, LV_ALIGN_TOP_MID, 0, h - 51);
    lv_obj_set_style_bg_color(boot.bar, lv_color_hex(0x173946), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(boot.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(boot.bar, lv_color_hex(cyan), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(boot.bar, LV_OPA_COVER, LV_PART_INDICATOR);
    boot.status = make_label("INITIALIZING PERIPHERALS", &lv_font_montserrat_12, cyan, h - 43);
    boot.note = make_label("", &lv_font_montserrat_12, muted, h - 27);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, boot.root);
    lv_anim_set_exec_cb(&animation, run_frame);
    lv_anim_set_values(&animation, 0, BOOT_MS);
    lv_anim_set_duration(&animation, BOOT_MS);
    lv_anim_set_completed_cb(&animation, finish);
    if (lv_anim_start(&animation)) return true;
    lv_obj_delete(boot.root);
    boot.root = NULL;
    return false;
}

void boot_screen_set_status(const char *status)
{
    if (boot.root && !boot.ready) lv_label_set_text(boot.status, status);
}

bool boot_screen_active(void)
{
    return boot.root != NULL;
}
