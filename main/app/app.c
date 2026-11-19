#include "app.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "board/touch.h"
#include "ui/port/lvgl_port.h"
#include "ui/port/touch_calibration.h"
#include "ui/ui.h"
#include "lvgl.h"

static QueueHandle_t s_updates;

static void ui_task(void *argument)
{
    (void)argument;
    ui_port_init();
    ui_touch_calibrate();
    ui_init(identity_demo(), board_touch_is_available());
    ESP_LOGI("app", "Identity UI ready; touch %s", board_touch_is_available() ? "enabled" : "not configured");
    for (;;) {
        identity_t update;
        if (xQueueReceive(s_updates, &update, 0) == pdTRUE) {
            ui_set_identity(&update);
        }
        uint32_t delay = lv_timer_handler();
        if (delay > 20) delay = 20;
        TickType_t ticks = pdMS_TO_TICKS(delay);
        vTaskDelay(ticks > 0 ? ticks : 1);
    }
}

void app_start(void)
{
    ESP_ERROR_CHECK(s_updates ? ESP_ERR_INVALID_STATE : ESP_OK);
    s_updates = xQueueCreate(1, sizeof(identity_t));
    ESP_ERROR_CHECK(s_updates ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(ui_task, "ui", 6144, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

esp_err_t app_set_identity(const identity_t *identity)
{
    if (!identity_is_valid(identity)) return ESP_ERR_INVALID_ARG;
    if (!s_updates) return ESP_ERR_INVALID_STATE;
    return xQueueOverwrite(s_updates, identity) == pdPASS ? ESP_OK : ESP_FAIL;
}
