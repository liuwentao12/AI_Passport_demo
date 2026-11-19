#include "microphone_test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

// 麦克风测试配置：Wi-Fi、I2S 引脚及录音参数。
#define USE_ESP32_AP 0
#define WIFI_SSID "Xiaomi 15 Ultra"
#define WIFI_PASSWORD "12345679"

#define PIN_I2S_BCLK GPIO_NUM_26
#define PIN_I2S_WS   GPIO_NUM_25
#define PIN_I2S_DIN  GPIO_NUM_33
#define SAMPLE_RATE 16000
#define DURATION_SECONDS 3
#define FRAME_SAMPLES 256
#define PCM_GAIN 4
#define WAV_DATA_BYTES (SAMPLE_RATE * DURATION_SECONDS * 2)
#define WAV_TOTAL_BYTES (WAV_DATA_BYTES + 44)

static const char *TAG = "phone-mic";
static i2s_chan_handle_t s_rx = NULL;
static EventGroupHandle_t s_wifi_event;
#define WIFI_CONNECTED_BIT BIT0

static const char INDEX_HTML[] =
"<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32 INMP441 录音</title>"
"<style>body{font-family:system-ui,-apple-system,sans-serif;max-width:560px;"
"margin:10vh auto;padding:18px;color:#17212b;background:#f5f7fa}"
"main{background:white;border-radius:18px;padding:24px;box-shadow:0 5px 30px #0001}"
"button{padding:14px 18px;border:0;border-radius:10px;background:#1769e0;color:white;"
"font-size:16px;cursor:pointer}button:disabled{opacity:.5}"
"audio{width:100%;margin:18px 0}a{display:inline-block;margin-top:12px}"
"small{color:#65717e}</style></head><body><main>"
"<h2>ESP32 / INMP441</h2><p>点击录音，向麦克风说话 3 秒，然后点击播放器试听。</p>"
"<button id='rec'>录制 3 秒</button><p id='status'>设备已就绪</p>"
"<audio id='audio' controls preload='none'></audio>"
"<p><a id='download' style='display:none' download='inmp441.wav'>保存 WAV 录音</a></p>"
"<small>16 kHz / 16-bit / 单声道 / 无需电脑与互联网</small>"
"</main><script>"
"let oldUrl=null;const button=document.getElementById('rec');"
"const status=document.getElementById('status');const player=document.getElementById('audio');"
"const download=document.getElementById('download');"
"button.onclick=async()=>{button.disabled=true;status.textContent='正在录制和接收，大约 3 秒…';"
"try{const response=await fetch('/record.wav?t='+Date.now(),{cache:'no-store'});"
"if(!response.ok)throw Error('HTTP '+response.status);const blob=await response.blob();"
"if(blob.size!==96044)throw Error('录音不完整：收到 '+blob.size+' 字节');"
"player.pause();if(oldUrl)URL.revokeObjectURL(oldUrl);"
"oldUrl=URL.createObjectURL(blob);player.src=oldUrl;download.href=oldUrl;"
"download.style.display='inline-block';status.textContent='录音完成。点击播放器 ▶ 试听';"
"}catch(e){status.textContent='失败：'+e.message;}finally{button.disabled=false;}};"
"</script></body></html>";

static void put_u16_le(uint8_t *p, uint16_t n)
{
    p[0] = n & 0xff;
    p[1] = (n >> 8) & 0xff;
}

static void put_u32_le(uint8_t *p, uint32_t n)
{
    p[0] = n & 0xff;
    p[1] = (n >> 8) & 0xff;
    p[2] = (n >> 16) & 0xff;
    p[3] = (n >> 24) & 0xff;
}

static void write_wav_header(uint8_t h[44])
{
    memset(h, 0, 44);
    memcpy(h, "RIFF", 4);
    put_u32_le(h + 4, WAV_DATA_BYTES + 36);
    memcpy(h + 8, "WAVEfmt ", 8);
    put_u32_le(h + 16, 16);             // PCM fmt chunk size
    put_u16_le(h + 20, 1);              // PCM (uncompressed)
    put_u16_le(h + 22, 1);              // Mono
    put_u32_le(h + 24, SAMPLE_RATE);
    put_u32_le(h + 28, SAMPLE_RATE * 2); // bytes/second
    put_u16_le(h + 32, 2);              // block align
    put_u16_le(h + 34, 16);             // sample bits
    memcpy(h + 36, "data", 4);
    put_u32_le(h + 40, WAV_DATA_BYTES);
}

static void i2s_init(void)
{
    i2s_chan_config_t channel_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&channel_cfg, NULL, &s_rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCLK,
            .ws = PIN_I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = PIN_I2S_DIN,
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
    ESP_LOGI(TAG, "I2S microphone ready: 16000 Hz, 32-bit stereo slots, LEFT microphone");
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t wav_handler(httpd_req_t *req)
{
    // Captures on demand; only buffers one small DMA block, not the entire recording.
    int32_t stereo[FRAME_SAMPLES * 2];
    int16_t mono[FRAME_SAMPLES];
    uint8_t wav_header[44];
    size_t bytes_read = 0;
    int remaining = SAMPLE_RATE * DURATION_SECONDS;
    int delivered = 0;

    // Restart the channel to discard any stale samples from a previous request.
    esp_err_t err = i2s_channel_disable(s_rx);
    if (err == ESP_OK) err = i2s_channel_enable(s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S reset failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "I2S reset failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "audio/wav");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    write_wav_header(wav_header);
    if (httpd_resp_send_chunk(req, (const char *)wav_header, sizeof(wav_header)) != ESP_OK) {
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int want = remaining < FRAME_SAMPLES ? remaining : FRAME_SAMPLES;
        err = i2s_channel_read(s_rx, stereo, want * 2 * sizeof(int32_t),
                               &bytes_read, pdMS_TO_TICKS(1500));
        if (err != ESP_OK || bytes_read == 0 || (bytes_read % 8) != 0) {
            ESP_LOGE(TAG, "I2S read failed: %s, %u bytes", esp_err_to_name(err), (unsigned)bytes_read);
            break;
        }
        int got = bytes_read / (2 * sizeof(int32_t));
        for (int i = 0; i < got; ++i) {
            // INMP441 L/R connected to GND: LEFT slot. 24-bit MSB-aligned data
            // inside a 32-bit signed slot. Keep the top 16 bits and add gain.
            int32_t sample = (stereo[i * 2] >> 16) * PCM_GAIN;
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            mono[i] = (int16_t)sample;
        }
        if (httpd_resp_send_chunk(req, (const char *)mono, got * sizeof(int16_t)) != ESP_OK) {
            ESP_LOGW(TAG, "HTTP client disconnected during recording");
            return ESP_FAIL;
        }
        remaining -= got;
        delivered += got;
    }
    ESP_LOGI(TAG, "WAV generated: %d / %d samples", delivered, SAMPLE_RATE * DURATION_SECONDS);
    // On a hardware read error, a truncated file is returned; the webpage detects its size.
    return httpd_resp_send_chunk(req, NULL, 0);
}

static void server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.stack_size = 8192;
    cfg.send_wait_timeout = 10;
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));
    httpd_uri_t home = {.uri = "/", .method = HTTP_GET, .handler = index_handler};
    httpd_uri_t wav = {.uri = "/record.wav", .method = HTTP_GET, .handler = wav_handler};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &home));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wav));
    ESP_LOGI(TAG, "HTTP server listening at port 80");
}

#if !USE_ESP32_AP
static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "Hotspot disconnected; reconnecting...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "ESP32 IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "PHONE BROWSER: http://" IPSTR "/", IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event, WIFI_CONNECTED_BIT);
    }
}
#endif

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
#if USE_ESP32_AP
    esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "ESP32-MIC",
            .password = "12345678",
            .ssid_len = 0,
            .channel = 1,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 2,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connect your PHONE to Wi-Fi ESP32-MIC (password 12345678)");
    ESP_LOGI(TAG, "PHONE BROWSER: http://192.168.4.1/");
#else
    s_wifi_event = xEventGroupCreate();
    ESP_ERROR_CHECK(s_wifi_event ? ESP_OK : ESP_ERR_NO_MEM);
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));
    wifi_config_t sta_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event, WIFI_CONNECTED_BIT,
                                          pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
    if ((bits & WIFI_CONNECTED_BIT) == 0) {
        ESP_LOGE(TAG, "Could not connect to hotspot within 30s. Check SSID/password and 2.4GHz.");
        return;
    }
#endif
}

void microphone_test_start(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    i2s_init();
    wifi_init();
#if USE_ESP32_AP
    server_start();
#else
    if (xEventGroupGetBits(s_wifi_event) & WIFI_CONNECTED_BIT) {
        server_start();
    }
#endif
}
