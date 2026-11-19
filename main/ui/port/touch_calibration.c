#include "touch_calibration.h"

#include <stdlib.h>
#include "board/touch.h"
#include "board/board_config.h"
#include "ui/ui_theme.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void pump(void)
{
    lv_timer_handler();
    TickType_t ticks = pdMS_TO_TICKS(20);
    vTaskDelay(ticks ? ticks : 1);
}

/* Require release before starting, five stable readings, then release again.
 * Each point times out after 60s so faulty wiring cannot block startup forever. */
static bool capture(lv_obj_t *hint, touch_point_t *out)
{
    int released = 0, count = 0;
    bool armed = false, captured = false;
    touch_point_t first = {0}, sum = {0};
    int64_t start = esp_timer_get_time();
    while (esp_timer_get_time() - start < 60000000) {
        pump();
        board_touch_sample_t raw;
        if (!board_touch_read_raw(&raw)) {
            released = 0;
            count = 0;
            continue;
        }
        if (!raw.pressed) {
            if (++released >= 3) {
                if (captured) return true;
                armed = true;
            }
            count = 0;
            continue;
        }
        released = 0;
        if (!armed || captured) continue;
        if (count && (abs(raw.x - first.x) > 80 || abs(raw.y - first.y) > 80)) count = 0;
        if (count == 0) {
            first = (touch_point_t){raw.x, raw.y};
            sum = (touch_point_t){0};
        }
        sum.x += raw.x;
        sum.y += raw.y;
        if (++count == 5) {
            *out = (touch_point_t){sum.x / count, sum.y / count};
            captured = true;
            lv_label_set_text(hint, "Release to continue");
        }
    }
    return false;
}

void ui_touch_calibrate(void)
{
    if (board_touch_is_available()) return;
    lv_obj_t *overlay = ui_box(lv_screen_active(), 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT,
                               UI_COLOR_BG, 0);
    lv_obj_t *title = ui_text(overlay, "Touch setup", 20, 74, 200,
                              &lv_font_montserrat_20, UI_COLOR_INK);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *hint = ui_text(overlay, "", 20, 109, 200, &lv_font_montserrat_14, UI_COLOR_MUTED);
    lv_obj_set_height(hint, 44);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *cross = ui_box(overlay, 0, 0, 23, 23, UI_COLOR_BG, 0);
    ui_box(cross, 10, 0, 3, 23, UI_COLOR_ACCENT, 0);
    ui_box(cross, 0, 10, 23, 3, UI_COLOR_ACCENT, 0);
    const touch_point_t *targets = board_touch_calibration_targets();

    for (;;) {
        touch_point_t raw[3], center;
        touch_transform_t transform;
        for (int i = 0; i < 3; ++i) {
            lv_label_set_text_fmt(title, "Touch setup %d/4", i + 1);
            lv_label_set_text(hint, "Hold the cross gently\nthen lift your finger");
            lv_obj_set_pos(cross, targets[i].x - 11, targets[i].y - 11);
            if (!capture(hint, &raw[i])) goto timeout;
            ESP_LOGI("touch-cal", "point %d raw=(%ld,%ld)", i + 1, (long)raw[i].x, (long)raw[i].y);
        }
        if (!touch_transform_fit(&transform, raw, targets)) {
            ESP_LOGW("touch-cal", "Invalid calibration triangle; retrying");
            lv_label_set_text(title, "Please retry");
            lv_label_set_text(hint, "Use each cross position");
            for (int i = 0; i < 50; ++i) pump();
            continue;
        }
        lv_label_set_text(title, "Touch setup 4/4");
        lv_label_set_text(hint, "Hold the center cross\nto check accuracy");
        lv_obj_set_pos(cross, BOARD_LCD_WIDTH / 2 - 11, BOARD_LCD_HEIGHT / 2 - 11);
        if (!capture(hint, &center)) goto timeout;
        touch_point_t pixel;
        if (!touch_transform_apply(&transform, center, &pixel) ||
            abs(pixel.x - BOARD_LCD_WIDTH / 2) > 15 || abs(pixel.y - BOARD_LCD_HEIGHT / 2) > 15) {
            ESP_LOGW("touch-cal", "Center check failed; retrying");
            lv_label_set_text(title, "Please retry");
            lv_label_set_text(hint, "Touch the cross precisely");
            for (int i = 0; i < 50; ++i) pump();
            continue;
        }
        esp_err_t err = board_touch_set_calibration(raw);
        if (err != ESP_OK) ESP_LOGW("touch-cal", "Calibration active, save failed: %s", esp_err_to_name(err));
        ESP_LOGI("touch-cal", "Calibration complete");
        break;
    }
    lv_obj_delete(overlay);
    return;

timeout:
    ESP_LOGW("touch-cal", "No completed tap within 60s; reboot to retry touch setup");
    lv_obj_delete(overlay);
}
