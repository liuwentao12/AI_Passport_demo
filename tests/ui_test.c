#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "ui/boot_screen.h"
#include "ui/idle_lock.h"
#include "ui/ui.h"
#include "ui/pages/wooden_fish_art.h"

static uint16_t frame[240 * 320];
static uint16_t buffer[240 * 20];
static unsigned strikes;
static unsigned home_preparations;
static unsigned flushes;
static bool dimmed;

static void record_strike(void)
{
    ++strikes;
}

static const char *prepare_home(void)
{
    ++home_preparations;
    ui_init(identity_demo(), false, record_strike);
    return "TOUCH SETUP REQUIRED";
}

static lv_obj_t *find_text(lv_obj_t *parent, const char *text)
{
    if (lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN)) return NULL;
    if (lv_obj_check_type(parent, &lv_label_class) &&
        strcmp(lv_label_get_text(parent), text) == 0) return parent;
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *found = find_text(lv_obj_get_child(parent, i), text);
        if (found) return found;
    }
    return NULL;
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    ++flushes;
    const int32_t width = lv_display_get_horizontal_resolution(display);
    const int32_t height = lv_display_get_vertical_resolution(display);
    uint16_t *src = (uint16_t *)pixels;
    for (int y = area->y1; y <= area->y2; ++y) {
        for (int x = area->x1; x <= area->x2; ++x) {
            assert(x >= 0 && x < width && y >= 0 && y < height);
            frame[y * width + x] = *src++;
        }
    }
    lv_display_flush_ready(display);
}

static void snapshot(const char *path)
{
    lv_refr_now(NULL);
    FILE *out = fopen(path, "wb");
    assert(out);
    fprintf(out, "P6\n240 320\n255\n");
    for (unsigned i = 0; i < sizeof(frame) / sizeof(frame[0]); ++i) {
        unsigned char rgb[3] = {
            (unsigned char)(((frame[i] >> 11) & 31) * 255 / 31),
            (unsigned char)(((frame[i] >> 5) & 63) * 255 / 63),
            (unsigned char)((frame[i] & 31) * 255 / 31),
        };
        fwrite(rgb, 1, 3, out);
    }
    fclose(out);
}

static void check_bounds(lv_obj_t *parent)
{
    lv_display_t *display = lv_obj_get_display(parent);
    const int32_t width = lv_display_get_horizontal_resolution(display);
    const int32_t height = lv_display_get_vertical_resolution(display);
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t bounds;
        lv_obj_get_coords(child, &bounds);
        assert(bounds.x1 >= 0 && bounds.y1 >= 0 && bounds.x2 < width && bounds.y2 < height);
        /* A scroll viewport must fit; its clipped content can extend below it. */
        if (!lv_obj_has_flag(child, LV_OBJ_FLAG_SCROLLABLE)) check_bounds(child);
    }
}

static const char *prepare_home_with_unavailable_peripherals(void)
{
    return "PERIPHERALS DEGRADED";
}

static void run_frames(unsigned duration)
{
    for (unsigned ms = 0; ms < duration; ms += 20) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
}

static void check_boot_screen(void)
{
    lv_obj_t *top = lv_layer_top();
    const uint32_t children = lv_obj_get_child_count(top);
    assert(boot_screen_start(prepare_home));
    boot_screen_set_status("INITIALIZING PERIPHERALS");
    lv_obj_update_layout(top);
    check_bounds(top);
    run_frames(2400);
    assert(boot_screen_active() && home_preparations == 1);
    assert(find_text(top, "SYSTEM READY"));
    assert(find_text(top, "TOUCH SETUP REQUIRED"));
    assert(find_text(top, "100%"));
    snapshot("boot.ppm");
    run_frames(440);
    assert(boot_screen_active());
    const lv_opa_t opacity = lv_obj_get_style_opa(lv_obj_get_child(top, -1), 0);
    assert(opacity > LV_OPA_TRANSP && opacity < LV_OPA_COVER);
    run_frames(260);
    assert(!boot_screen_active() && home_preparations == 1);
    assert(lv_obj_get_child_count(top) == children);

    /* Exercise the landscape layout independently of the portrait-only home pages. */
    lv_display_t *landscape = lv_display_create(320, 240);
    lv_display_set_default(landscape);
    lv_display_set_color_format(landscape, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(landscape, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(landscape, flush);
    assert(boot_screen_start(prepare_home_with_unavailable_peripherals));
    run_frames(2400);
    check_bounds(lv_layer_top());
    assert(find_text(lv_layer_top(), "PERIPHERALS DEGRADED"));
    run_frames(700);
    assert(!boot_screen_active() && lv_obj_get_child_count(lv_layer_top()) == 0);
    lv_display_delete(landscape);
    lv_display_set_default(lv_obj_get_display(top));
}

static void set_dimmed(bool value)
{
    dimmed = value;
}

static void check_idle_lock(void)
{
    lv_obj_t *top = lv_layer_top();
    ui_show_page(UI_PAGE_WOODEN_FISH);
    idle_lock_start(set_dimmed);
    lv_tick_inc(59000);
    assert(!idle_lock_handle_input());
    lv_tick_inc(59999);
    idle_lock_poll();
    assert(!dimmed && !find_text(top, "SCREEN LOCKED"));
    lv_tick_inc(1);
    idle_lock_poll();
    assert(dimmed && find_text(top, "SCREEN LOCKED"));
    check_bounds(top);
    assert(frame[0] != 0 && frame[0] < 0x2104);
    const unsigned lock_flushes = flushes;
    ui_set_identity(identity_demo());
    lv_obj_invalidate(lv_screen_active());
    run_frames(100);
    assert(flushes == lock_flushes);
    assert(idle_lock_handle_input());
    assert(!dimmed && !find_text(top, "SCREEN LOCKED"));
    assert(!idle_lock_handle_input());
    run_frames(100);
    assert(flushes > lock_flushes && ui_current_page() == UI_PAGE_WOODEN_FISH);
    ui_show_page(UI_PAGE_IDENTITY);
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    check_boot_screen();
    check_idle_lock();
    assert(ui_current_page() == UI_PAGE_IDENTITY);
    lv_obj_t *hint = find_text(lv_screen_active(), "TOUCH NOT CONFIGURED");
    assert(hint);
    static const char long_status[] =
        "NFC sharing status that is deliberately longer than the width of the footer label";
    ui_set_nfc_status(long_status);
    lv_obj_update_layout(lv_screen_active());
    assert(lv_label_get_text(hint) != long_status);
    ui_show_page(UI_PAGE_SETTINGS);
    assert(strcmp(lv_label_get_text(hint), "TOUCH NOT CONFIGURED") == 0);
    ui_show_page(UI_PAGE_IDENTITY);
    assert(strncmp(lv_label_get_text(hint), "NFC sharing", 11) == 0);
    ui_set_nfc_status(NULL);
    assert(strcmp(lv_label_get_text(hint), "TOUCH NOT CONFIGURED") == 0);
    snapshot("identity.ppm");
    check_bounds(lv_screen_active());
    uint32_t children = lv_obj_get_child_count(lv_screen_active());
    lv_obj_t *details_button = lv_obj_get_parent(find_text(lv_screen_active(), "Details"));
    lv_obj_t *identity_button = lv_obj_get_parent(find_text(lv_screen_active(), "Identity"));
    lv_mem_monitor_t before, after;
    lv_mem_monitor(&before);
    for (int i = 0; i < 100; ++i) {
        lv_obj_send_event(details_button, LV_EVENT_CLICKED, NULL);
        assert(ui_current_page() == UI_PAGE_DETAILS);
        lv_obj_send_event(identity_button, LV_EVENT_CLICKED, NULL);
    }
    assert(lv_obj_get_child_count(lv_screen_active()) == children);
    lv_mem_monitor(&after);
    assert(after.free_size == before.free_size);
    const char *tabs[] = {"Muyu", "AI Chat", "Settings"};
    const ui_page_t destinations[] = {UI_PAGE_WOODEN_FISH, UI_PAGE_AI_CHAT, UI_PAGE_SETTINGS};
    for (unsigned i = 0; i < sizeof(tabs) / sizeof(tabs[0]); ++i) {
        lv_obj_t *label = find_text(lv_screen_active(), tabs[i]);
        assert(label);
        lv_obj_send_event(lv_obj_get_parent(label), LV_EVENT_CLICKED, NULL);
        assert(ui_current_page() == destinations[i]);
        lv_obj_update_layout(lv_screen_active());
        check_bounds(lv_screen_active());
    }
    ui_show_page(UI_PAGE_WOODEN_FISH);
    lv_obj_t *counter = find_text(lv_screen_active(), "0");
    assert(counter);
    lv_obj_t *page = lv_obj_get_parent(counter);
    lv_obj_t *fish = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(page); ++i) {
        lv_obj_t *child = lv_obj_get_child(page, i);
        if (lv_obj_check_type(child, &lv_image_class) && lv_image_get_src(child) == &wooden_fish_art) {
            fish = child;
            break;
        }
    }
    assert(fish);
    lv_obj_send_event(fish, LV_EVENT_CLICKED, NULL);
    lv_obj_send_event(fish, LV_EVENT_CLICKED, NULL);
    ui_show_page(UI_PAGE_SETTINGS);
    ui_show_page(UI_PAGE_WOODEN_FISH);
    assert(strikes == 2 && strcmp(lv_label_get_text(counter), "2") == 0);
    ui_show_page(UI_PAGE_DETAILS);
    snapshot("details.ppm");
    check_bounds(lv_screen_active());
    identity_t longest = *identity_demo();
    memset(longest.name, 'W', sizeof(longest.name) - 1);
    memset(longest.description, 'W', sizeof(longest.description) - 1);
    ui_set_identity(&longest);
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *full_name = find_text(lv_screen_active(), longest.name);
    lv_obj_t *full_description = find_text(lv_screen_active(), longest.description);
    assert(full_name && full_description);
    assert(lv_label_get_long_mode(full_name) == LV_LABEL_LONG_WRAP);
    assert(lv_label_get_long_mode(full_description) == LV_LABEL_LONG_WRAP);
    assert(lv_obj_get_scroll_bottom(lv_obj_get_parent(full_description)) > 0);
    assert(find_text(lv_screen_active(), longest.number));
    assert(find_text(lv_screen_active(), longest.role));
    check_bounds(lv_screen_active());
    ui_show_page(UI_PAGE_IDENTITY);
    lv_obj_update_layout(lv_screen_active());
    check_bounds(lv_screen_active());
    ui_show_page((ui_page_t)999);
    assert(ui_current_page() == UI_PAGE_IDENTITY);
    puts("UI layout, data update and 100 page round trips passed.");
    return 0;
}
