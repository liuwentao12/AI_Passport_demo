#include "siliconflow.h"
#include "voice_config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"

#define API_ROOT "https://api.siliconflow.cn/v1"
#define BOUNDARY "----AIPassportWave7D19"
#define RESPONSE_LIMIT 4096

typedef struct { const void *data; size_t length; } body_part_t;

static esp_err_t write_part(esp_http_client_handle_t client, body_part_t part)
{
    const char *data = part.data;
    while (part.length) {
        int length = part.length > 4096 ? 4096 : (int)part.length;
        int written = esp_http_client_write(client, data, length);
        if (written <= 0) return ESP_FAIL;
        data += written;
        part.length -= written;
    }
    return ESP_OK;
}

static const char *http_error(int status)
{
    switch (status) {
    case 401: return "Check API key";
    case 402: return "Check API balance";
    case 403: return "Check API permissions";
    case 404: return "Model unavailable";
    case 429: return "Rate limited. Try again";
    default: return "SiliconFlow request failed";
    }
}

static const char *post(const char *url, const char *type, const body_part_t *parts,
                        size_t count, void *owned_upload, cJSON **json)
{
    static const char auth[] = "Bearer " VOICE_SILICONFLOW_KEY;
    const esp_http_client_config_t config = {
        .url = url, .method = HTTP_METHOD_POST, .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach, .disable_auto_redirect = true,
        .buffer_size = 1024, .buffer_size_tx = 1024,
    };
    *json = NULL;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    size_t total = 0;
    for (size_t i = 0; i < count; ++i) total += parts[i].length;

    esp_err_t err = client ? esp_http_client_set_header(client, "Authorization", auth) : ESP_ERR_NO_MEM;
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Content-Type", type);
    if (err == ESP_OK) err = esp_http_client_open(client, (int)total);
    for (size_t i = 0; err == ESP_OK && i < count; ++i) err = write_part(client, parts[i]);
    free(owned_upload);

    const char *failure = "HTTPS failed. Check hotspot";
    char *body = NULL;
    if (err != ESP_OK) goto done;
    int64_t length = esp_http_client_fetch_headers(client);
    if (length < 0) goto done;
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW("siliconflow", "HTTP %d from %s", status, url);
        failure = http_error(status);
        goto done;
    }
    failure = "API response too large";
    if (length > RESPONSE_LIMIT) goto done;
    body = malloc(RESPONSE_LIMIT + 1);
    if (!body) {
        failure = "Not enough memory";
        goto done;
    }
    int used = 0;
    int64_t deadline = esp_timer_get_time() + INT64_C(20000000);
    for (;;) {
        failure = "API response timed out";
        if (esp_timer_get_time() >= deadline) goto done;
        int room = RESPONSE_LIMIT + 1 - used;
        int got = esp_http_client_read(client, body + used, room > 1024 ? 1024 : room);
        if (got == 0 && esp_http_client_is_complete_data_received(client)) break;
        if (got <= 0) goto done;
        used += got;
        failure = "API response too large";
        if (used > RESPONSE_LIMIT) goto done;
    }
    body[used] = '\0';
    esp_http_client_cleanup(client);
    client = NULL;
    *json = cJSON_ParseWithLengthOpts(body, (size_t)used + 1, NULL, true);
    failure = *json ? NULL : "Invalid API response";

done:
    if (err != ESP_OK) ESP_LOGW("siliconflow", "HTTPS request failed: %s", esp_err_to_name(err));
    if (client) esp_http_client_cleanup(client);
    free(body);
    return failure;
}

static bool copy_text(char *out, size_t capacity, const cJSON *value)
{
    const char *text = cJSON_GetStringValue(value);
    if (!text) return false;
    text += strspn(text, " \t\r\n");
    if (!*text) return false;
    size_t length = strlen(text);
    if (length >= capacity) {
        length = capacity - 1;
        /* Cut before the next UTF-8 code point, never inside one. */
        while (length && ((unsigned char)text[length] & 0xc0) == 0x80) --length;
    }
    memcpy(out, text, length);
    out[length] = '\0';
    return length != 0;
}

const char *siliconflow_transcribe(void *wav, size_t bytes, char *text, size_t capacity)
{
    static const char prefix[] =
        "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n"
        "FunAudioLLM/SenseVoiceSmall\r\n"
        "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    static const char suffix[] = "\r\n--" BOUNDARY "--\r\n";
    const body_part_t parts[] = {{prefix, sizeof(prefix) - 1}, {wav, bytes}, {suffix, sizeof(suffix) - 1}};
    cJSON *json;
    const char *failure = post(API_ROOT "/audio/transcriptions", "multipart/form-data; boundary=" BOUNDARY,
                               parts, sizeof(parts) / sizeof(parts[0]), wav, &json);
    if (!failure && !copy_text(text, capacity, cJSON_GetObjectItemCaseSensitive(json, "text"))) {
        failure = "No speech recognized";
    }
    cJSON_Delete(json);
    return failure;
}

static bool add_message(cJSON *messages, const char *role, const char *content)
{
    cJSON *item = cJSON_CreateObject();
    if (!item) return false;
    if (!cJSON_AddStringToObject(item, "role", role) ||
        !cJSON_AddStringToObject(item, "content", content) || !cJSON_AddItemToArray(messages, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

const char *siliconflow_answer(const char *text, char *answer, size_t capacity)
{
    cJSON *request = cJSON_CreateObject();
    if (!request) return "Not enough memory";
    cJSON *messages = cJSON_AddArrayToObject(request, "messages");
    char *body = NULL;
    if (messages && cJSON_AddStringToObject(request, "model", VOICE_CHAT_MODEL) &&
        cJSON_AddNumberToObject(request, "max_tokens", 120) &&
        cJSON_AddBoolToObject(request, "stream", false) &&
        cJSON_AddBoolToObject(request, "enable_thinking", false) &&
        add_message(messages, "system", "Reply in the user's language, in plain text, using at most two short sentences.") &&
        add_message(messages, "user", text)) {
        body = cJSON_PrintUnformatted(request);
    }
    cJSON_Delete(request);
    if (!body) return "Not enough memory";

    const body_part_t part = {body, strlen(body)};
    cJSON *json;
    /* cJSON uses the default allocator in this application. */
    const char *failure = post(API_ROOT "/chat/completions", "application/json", &part, 1, body, &json);
    const cJSON *choices = cJSON_GetObjectItemCaseSensitive(json, "choices");
    const cJSON *message = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(choices, 0), "message");
    if (!failure && !copy_text(answer, capacity, cJSON_GetObjectItemCaseSensitive(message, "content"))) {
        failure = "No answer returned";
    }
    cJSON_Delete(json);
    return failure;
}
