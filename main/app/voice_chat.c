#include "voice_chat.h"
#include "voice_config.h"
#include "siliconflow.h"
#include "board/microphone.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#define MAX_SAMPLES (MIC_SAMPLE_RATE * 5 / 2)
#define MIN_SAMPLES (MIC_SAMPLE_RATE / 4)
#define GOT_IP_BIT BIT0
#define DISCONNECTED_BIT BIT1

static const char *TAG = "voice-chat";
static QueueHandle_t s_updates;
static EventGroupHandle_t s_wifi_events;
static bool s_worker_active;
static atomic_bool s_stop;
static atomic_bool s_recording;

static void wifi_event(void *context, esp_event_base_t base, int32_t id, void *data)
{
    (void)context;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_events, GOT_IP_BIT);
        xEventGroupSetBits(s_wifi_events, DISCONNECTED_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupClearBits(s_wifi_events, DISCONNECTED_BIT);
        xEventGroupSetBits(s_wifi_events, GOT_IP_BIT);
    }
}

static esp_err_t wifi_init_once(void)
{
    static bool attempted;
    static esp_err_t result;
    if (attempted) return result;
    attempted = true;

    result = nvs_flash_init();
    if (result == ESP_OK) result = esp_netif_init();
    if (result == ESP_OK) result = esp_event_loop_create_default();
    if (result != ESP_OK) return result;
    const size_t ssid_len = strlen(VOICE_WIFI_SSID);
    const size_t password_len = strlen(VOICE_WIFI_PASSWORD);
    wifi_config_t config = {0};
    if (!ssid_len || ssid_len > sizeof(config.sta.ssid) || password_len >= sizeof(config.sta.password)) {
        return result = ESP_ERR_INVALID_ARG;
    }
    memcpy(config.sta.ssid, VOICE_WIFI_SSID, ssid_len);
    memcpy(config.sta.password, VOICE_WIFI_PASSWORD, password_len);
    if (!esp_netif_create_default_wifi_sta()) return result = ESP_FAIL;
    s_wifi_events = xEventGroupCreate();
    if (!s_wifi_events) return result = ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&init);
    if (result == ESP_OK) result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result == ESP_OK) result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event, NULL);
    if (result == ESP_OK) result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL);
    if (result == ESP_OK) result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result == ESP_OK) result = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (result == ESP_OK) result = esp_wifi_start();
    return result;
}

static esp_err_t connect_hotspot(void)
{
    ESP_RETURN_ON_ERROR(wifi_init_once(), TAG, "Wi-Fi initialization failed");
    if (xEventGroupGetBits(s_wifi_events) & GOT_IP_BIT) return ESP_OK;

    xEventGroupClearBits(s_wifi_events, DISCONNECTED_BIT);
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "Wi-Fi connect failed");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, GOT_IP_BIT | DISCONNECTED_BIT,
                                          pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    if (bits & GOT_IP_BIT) return ESP_OK;
    esp_wifi_disconnect();
    return ESP_ERR_TIMEOUT;
}

static esp_err_t sync_clock(void)
{
    if (time(NULL) >= 1704067200) return ESP_OK; /* Clock has been set since boot. */
    const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&config), TAG, "SNTP init failed");
    esp_err_t err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(12000));
    esp_netif_sntp_deinit();
    return err;
}

static void publish(voice_chat_update_t *update, const char *status, bool finished)
{
    update->status = status;
    update->finished = finished;
    xQueueOverwrite(s_updates, update);
}

static void run_chat(void *argument)
{
    (void)argument;
    voice_chat_update_t update = {0};
    uint8_t *wav = malloc(MIC_WAV_HEADER_SIZE + MAX_SAMPLES * sizeof(int16_t));
    const char *status = "Not enough recording memory";
    if (!wav) goto done;
    esp_err_t err = microphone_begin();
    status = "Microphone failed";
    if (err != ESP_OK) goto done;
    publish(&update, "Recording. KEY3 to stop", false);

    int16_t *pcm = (int16_t *)(wav + MIC_WAV_HEADER_SIZE);
    size_t count = 0;
    while (count < MAX_SAMPLES && !atomic_load(&s_stop)) {
        size_t samples;
        err = microphone_read(pcm + count, MAX_SAMPLES - count, &samples);
        if (err != ESP_OK) break;
        count += samples;
    }
    atomic_store(&s_recording, false);
    microphone_end();
    if (err != ESP_OK) goto done;
    status = "Hold your question for longer";
    if (count < MIN_SAMPLES) goto done;

    const size_t pcm_bytes = count * sizeof(int16_t);
    microphone_wav_header(wav, pcm_bytes);
    publish(&update, "Connecting to hotspot...", false);
    status = "Check 2.4 GHz hotspot";
    if (connect_hotspot() != ESP_OK) goto done;
    status = "Clock sync failed. Try again";
    if (sync_clock() != ESP_OK) goto done;

    publish(&update, "Recognizing speech...", false);
    status = siliconflow_transcribe(wav, MIC_WAV_HEADER_SIZE + pcm_bytes,
                                    update.question, sizeof(update.question));
    wav = NULL;
    if (status) goto done;
    publish(&update, "Waiting for AI...", false);
    status = siliconflow_answer(update.question, update.answer, sizeof(update.answer));
    if (!status) status = "KEY3 to ask again";

done:
    free(wav);
    atomic_store(&s_recording, false);
    publish(&update, status, true);
    vTaskDelete(NULL);
}

void voice_chat_press(void)
{
    if (!s_updates) s_updates = xQueueCreate(1, sizeof(voice_chat_update_t));
    if (!s_updates) return;
    if (s_worker_active) {
        if (atomic_load(&s_recording)) atomic_store(&s_stop, true);
        return;
    }
    voice_chat_update_t update = {0};
    if (!VOICE_WIFI_SSID[0] || !VOICE_SILICONFLOW_KEY[0]) {
        publish(&update, "Set hotspot and API key", true);
        return;
    }
    atomic_store(&s_stop, false);
    atomic_store(&s_recording, true);
    publish(&update, "Starting microphone...", false);
    s_worker_active = xTaskCreate(run_chat, "voice_chat", 8192, NULL, 4, NULL) == pdPASS;
    if (!s_worker_active) {
        atomic_store(&s_recording, false);
        publish(&update, "Not enough task memory", true);
    }
}

bool voice_chat_poll(voice_chat_update_t *update)
{
    if (!s_updates || xQueueReceive(s_updates, update, 0) != pdTRUE) return false;
    if (update->finished) s_worker_active = false;
    return true;
}
