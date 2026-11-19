#include "app.h"
#include "voice_chat.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "board/display.h"
#include "board/touch.h"
#include "board/keys.h"
#include "board/buzzer.h"
#include "board/pn532.h"
#include "ui/boot_screen.h"
#include "ui/idle_lock.h"
#include "ui/port/lvgl_port.h"
#include "ui/port/touch_calibration.h"
#include "ui/ui.h"
#include "ui/pages/voice_chat_page.h"
#include "lvgl.h"

static QueueHandle_t s_updates;
static QueueHandle_t s_nfc_updates;
static bool s_nfc_busy;
static bool s_nfc_ready;
static const char s_share_url[] = "https://github.com/liuwentao12";

typedef struct {
    const char *text;
    bool finished;
} nfc_update_t;

static void nfc_ready(void)
{
    s_nfc_ready = true;
    const nfc_update_t update = {"Tap phone near NFC", false};
    xQueueOverwrite(s_nfc_updates, &update);
}

static void nfc_task(void *argument)
{
    (void)argument;
    s_nfc_ready = false;
    bool read;
    const esp_err_t err = pn532_share_url(s_share_url, nfc_ready, &read);
    const char *status;
    if (read) status = "GitHub link shared";
    else if (s_nfc_ready && err == ESP_ERR_TIMEOUT) status = "NFC timed out. Press KEY1";
    else {
        ESP_LOGW("app", "PN532 session failed: %s", esp_err_to_name(err));
        status = "Check PN532 and wiring";
    }
    const nfc_update_t update = {status, true};
    xQueueOverwrite(s_nfc_updates, &update);
    vTaskDelete(NULL);
}

static void start_nfc_share(void)
{
    if (s_nfc_busy) return;
    if (!s_nfc_updates) s_nfc_updates = xQueueCreate(1, sizeof(nfc_update_t));
    if (!s_nfc_updates || xTaskCreate(nfc_task, "nfc", 4096, NULL, 4, NULL) != pdPASS) {
        ui_set_nfc_status("NFC unavailable. Try again");
        return;
    }
    s_nfc_busy = true;
    ui_set_nfc_status("Starting NFC...");
}

static const ui_page_t s_key_pages[BOARD_KEY_COUNT] = {
    [BOARD_KEY1] = UI_PAGE_IDENTITY,
    [BOARD_KEY2] = UI_PAGE_WOODEN_FISH,
    [BOARD_KEY3] = UI_PAGE_AI_CHAT,
    [BOARD_KEY4] = UI_PAGE_SETTINGS,
};

enum {
    PERIPHERALS_DONE = 1U << 0,
    KEYS_READY = 1U << 1,
    BUZZER_READY = 1U << 2,
};

/* Only the UI task reads or writes the startup state below. */
static uint32_t s_peripherals;
static bool s_touch_initialized;

static void peripheral_init_task(void *argument)
{
    TaskHandle_t ui = argument;
    uint32_t result = PERIPHERALS_DONE;
    const esp_err_t buzzer = board_buzzer_init();
    if (buzzer == ESP_OK) result |= BUZZER_READY;
    else ESP_LOGW("app", "Buzzer unavailable: %s", esp_err_to_name(buzzer));

    const esp_err_t keys = board_keys_init();
    if (keys == ESP_OK) result |= KEYS_READY;
    else ESP_LOGW("app", "Keys disabled: %s", esp_err_to_name(keys));
    xTaskNotify(ui, result, eSetBits);
    vTaskDelete(NULL);
}

static bool touch_available(void)
{
    return s_touch_initialized && board_touch_is_available();
}

static void strike_if_ready(void)
{
    if (s_peripherals & BUZZER_READY) board_buzzer_strike();
}

static const char *prepare_home(void)
{
    ui_init(identity_demo(), touch_available(), strike_if_ready);
    if (!(s_peripherals & PERIPHERALS_DONE)) return "PERIPHERALS STILL STARTING";
    if (!s_touch_initialized || !(s_peripherals & KEYS_READY) ||
        !(s_peripherals & BUZZER_READY)) return "PERIPHERALS DEGRADED";
    if (!touch_available()) return "TOUCH SETUP REQUIRED";
    return NULL;
}

static void set_display_dimmed(bool dimmed)
{
    const esp_err_t err = board_display_set_dimmed(dimmed);
    if (err != ESP_OK) ESP_LOGW("app", "Backlight update failed: %s", esp_err_to_name(err));
}

static void ui_task(void *argument)
{
    (void)argument;
    const esp_err_t touch = ui_port_init();
    s_touch_initialized = touch == ESP_OK;
    if (touch != ESP_OK) ESP_LOGW("app", "Touch disabled: %s", esp_err_to_name(touch));

    if (xTaskCreate(peripheral_init_task, "peripheral_init", 3072,
                    xTaskGetCurrentTaskHandle(), 3, NULL) != pdPASS) {
        s_peripherals = PERIPHERALS_DONE;
        ESP_LOGW("app", "Could not start peripheral initialization task");
    }
    if (!boot_screen_start(prepare_home)) {
        ESP_LOGW("app", "Could not start boot animation");
        prepare_home();
    }
    if (s_peripherals == PERIPHERALS_DONE) {
        boot_screen_set_status("PERIPHERALS UNAVAILABLE");
    }

    bool calibrate = s_touch_initialized && !touch_available();
    bool idle_started = false;
    for (;;) {
        uint32_t result;
        if (xTaskNotifyWait(0, UINT32_MAX, &result, 0) == pdTRUE) {
            s_peripherals = result;
            boot_screen_set_status((result & (KEYS_READY | BUZZER_READY)) == (KEYS_READY | BUZZER_READY)
                                   ? "KEYS AND BUZZER CONFIGURED" : "PERIPHERALS DEGRADED");
        }
        if (!boot_screen_active()) {
            if (calibrate) {
                ui_touch_calibrate();
                ui_set_touch_available(touch_available());
                calibrate = false;
            }
            if (!idle_started && (touch_available() || (s_peripherals & KEYS_READY))) {
                idle_lock_start(set_display_dimmed);
                idle_started = true;
            }
            if (s_peripherals & BUZZER_READY) board_buzzer_poll();
            nfc_update_t nfc;
            if (s_nfc_updates && xQueueReceive(s_nfc_updates, &nfc, 0) == pdTRUE) {
                ui_set_nfc_status(nfc.text);
                if (nfc.finished) s_nfc_busy = false;
            }
            voice_chat_update_t voice;
            if (voice_chat_poll(&voice)) voice_chat_page_update(&voice);
            identity_t update;
            if (xQueueReceive(s_updates, &update, 0) == pdTRUE) {
                ui_set_identity(&update);
            }
            uint32_t presses = (s_peripherals & KEYS_READY) ? board_keys_poll() : 0;
            if (presses && idle_lock_handle_input()) presses = 0;
            for (int key = 0; key < BOARD_KEY_COUNT; ++key) {
                if (presses & (UINT32_C(1) << key)) {
                    ui_show_page(s_key_pages[key]);
                    if (key == BOARD_KEY1) start_nfc_share();
                    if (key == BOARD_KEY3) voice_chat_press();
                    break;
                }
            }
        }
        uint32_t delay = lv_timer_handler();
        idle_lock_poll();
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
