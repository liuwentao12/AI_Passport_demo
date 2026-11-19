#include "pn532.h"
#include "board_config.h"

#include <stdint.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FRAME_DATA_MAX 254
#define NDEF_FILE_CAPACITY 128
#define TAG_WINDOW_US INT64_C(20000000)
#define HSU_WAKE_BYTES 32

/* UM0701 host link uses normal information frames, direction D4/D5. */
static const uint8_t s_ack[] = {0x00, 0x00, 0xff, 0x00, 0xff, 0x00};
static const char *TAG = "pn532";

static esp_err_t read_exact(uint8_t *out, size_t size, int64_t deadline)
{
    while (size) {
        const int64_t remaining = deadline - esp_timer_get_time();
        if (remaining <= 0) return ESP_ERR_TIMEOUT;
        const TickType_t ticks = pdMS_TO_TICKS((remaining + 999) / 1000);
        int got = uart_read_bytes(BOARD_NFC_UART, out, size, ticks ? ticks : 1);
        if (got < 0) return ESP_FAIL;
        if (got == 0) continue;
        out += got;
        size -= got;
    }
    return ESP_OK;
}

static esp_err_t begin_command(uint8_t command, const uint8_t *args, size_t count)
{
    if (count > FRAME_DATA_MAX - 2) return ESP_ERR_INVALID_SIZE;
    const uint8_t length = count + 2;
    /* The first SAMConfiguration must wake PN532 from LowVbat. Send the
     * long HSU preamble and the command as a single UART write (UM0701 Fig. 65). */
    uint8_t buffer[HSU_WAKE_BYTES + FRAME_DATA_MAX + 7] = {0};
    const size_t prefix = command == 0x14 ? HSU_WAKE_BYTES : 0;
    if (prefix) buffer[0] = buffer[1] = 0x55;
    uint8_t *frame = buffer + prefix;
    frame[2] = 0xff;
    frame[3] = length;
    frame[4] = (uint8_t)(-length);
    frame[5] = 0xd4;
    frame[6] = command;
    if (count) memcpy(frame + 7, args, count);
    uint8_t sum = 0xd4 + command;
    for (size_t i = 0; i < count; ++i) sum += args[i];
    frame[7 + count] = (uint8_t)(-sum);
    frame[8 + count] = 0;
    const int frame_size = prefix + count + 9;
    if (uart_write_bytes(BOARD_NFC_UART, buffer, frame_size) != frame_size) {
        ESP_LOGW(TAG, "cmd 0x%02x UART write failed", command);
        return ESP_FAIL;
    }
    uint8_t ack[sizeof(s_ack)];
    esp_err_t err = read_exact(ack, sizeof(ack), esp_timer_get_time() + 250000);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cmd 0x%02x waiting for ACK: %s", command, esp_err_to_name(err));
        return err;
    }
    if (memcmp(ack, s_ack, sizeof(s_ack))) {
        ESP_LOGW(TAG, "cmd 0x%02x unexpected ACK frame", command);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t response(uint8_t command, uint8_t *out, size_t *size, int64_t deadline)
{
    uint8_t header[5];
    esp_err_t err = read_exact(header, sizeof(header), deadline);
    if (err != ESP_OK) {
        /* A timeout waiting for a phone to approach or leave is normal. */
        if (err != ESP_ERR_TIMEOUT || (command != 0x8c && command != 0x86)) {
            ESP_LOGW(TAG, "cmd 0x%02x waiting for response: %s", command, esp_err_to_name(err));
        }
        return err;
    }
    const uint8_t length = header[3];
    if (header[0] || header[1] || header[2] != 0xff || length < 2 || length > FRAME_DATA_MAX ||
        (uint8_t)(length + header[4]) != 0) {
        ESP_LOGW(TAG, "cmd 0x%02x invalid response header", command);
        return ESP_ERR_INVALID_RESPONSE;
    }
    uint8_t frame[FRAME_DATA_MAX + 2];
    err = read_exact(frame, length + 2U, deadline);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cmd 0x%02x incomplete UART response: %s", command, esp_err_to_name(err));
        return err;
    }
    if (length - 2U > *size) return ESP_ERR_INVALID_SIZE;
    uint8_t sum = 0;
    for (size_t i = 0; i <= length; ++i) sum += frame[i];
    if (sum || frame[length + 1] || frame[0] != 0xd5 || frame[1] != command + 1) {
        ESP_LOGW(TAG, "cmd 0x%02x invalid response frame", command);
        return ESP_ERR_INVALID_RESPONSE;
    }
    *size = length - 2;
    memcpy(out, frame + 2, *size);
    return ESP_OK;
}

static esp_err_t command(uint8_t code, const uint8_t *args, size_t count,
                         uint8_t *out, size_t *size, int64_t deadline)
{
    esp_err_t err = begin_command(code, args, count);
    if (err != ESP_OK) return err;
    return response(code, out, size, deadline);
}

typedef enum { FILE_NONE, FILE_CC, FILE_NDEF } tag_file_t;

static size_t tag_reply(tag_file_t *file, bool *application_selected, const uint8_t *apdu, size_t size,
                        const uint8_t *ndef, size_t ndef_size, uint8_t *out, bool *read_end)
{
    static const uint8_t aid[] = {0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
    static const uint8_t cc[] = {0x00, 0x0f, 0x20, 0x00, 0x80, 0x00, 0x7f,
                                0x04, 0x06, 0xe1, 0x04, 0x00, NDEF_FILE_CAPACITY, 0x00, 0xff};
    uint16_t status = 0x6700;
    size_t count = 0;
    *read_end = false;
    if (size < 4) goto done;
    if (apdu[0] != 0x00) {
        status = 0x6e00;
        goto done;
    }

    switch (apdu[1]) {
    case 0xa4: /* SELECT */
        if (size < 5 || (size != 5U + apdu[4] && size != 6U + apdu[4])) break;
        if (apdu[2] == 0x04) {
            *file = FILE_NONE;
            *application_selected = apdu[4] == sizeof(aid) && !memcmp(apdu + 5, aid, sizeof(aid));
            status = *application_selected ? 0x9000 : 0x6a82;
        } else if (apdu[2] == 0x00 && apdu[4] == 2) {
            *file = FILE_NONE;
            if (!*application_selected || apdu[5] != 0xe1) {
                status = 0x6a82;
                break;
            }
            if (apdu[6] == 0x03) *file = FILE_CC;
            if (apdu[6] == 0x04) *file = FILE_NDEF;
            status = *file != FILE_NONE ? 0x9000 : 0x6a82;
        } else {
            status = 0x6a86;
        }
        break;
    case 0xb0: { /* READ BINARY */
        if (size != 5) break;
        if (*file == FILE_NONE) {
            status = 0x6986;
            break;
        }
        const uint8_t *data = *file == FILE_CC ? cc : ndef;
        const size_t total = *file == FILE_CC ? sizeof(cc) : ndef_size;
        size_t offset = ((size_t)apdu[2] << 8) | apdu[3];
        if (offset >= total) {
            status = 0x6b00;
            break;
        }
        size_t requested = apdu[4] ? apdu[4] : 256;
        count = requested > total - offset ? total - offset : requested;
        memcpy(out, data + offset, count);
        status = count == requested ? 0x9000 : 0x6282;
        *read_end = *file == FILE_NDEF && offset + count == total;
        break;
    }
    case 0xd6: /* UPDATE BINARY */
        status = 0x6982;
        break;
    default:
        status = 0x6d00;
        break;
    }

done:
    out[count++] = status >> 8;
    out[count++] = status;
    return count;
}

static void abort_command(void)
{
    /* Wake a sleeping target over HSU, then send a host ACK to abort. */
    const uint8_t preamble[HSU_WAKE_BYTES] = {0x55, 0x55};
    uart_write_bytes(BOARD_NFC_UART, preamble, sizeof(preamble));
    uart_wait_tx_done(BOARD_NFC_UART, pdMS_TO_TICKS(50));
    vTaskDelay(2);
    uart_write_bytes(BOARD_NFC_UART, s_ack, sizeof(s_ack));
    uart_wait_tx_done(BOARD_NFC_UART, pdMS_TO_TICKS(50));
    vTaskDelay(2);
    uart_flush_input(BOARD_NFC_UART);
}

static esp_err_t emulate(const uint8_t *ndef, size_t ndef_size, void (*on_ready)(void), bool *read)
{
    /* Passive ISO 14443-4 PICC. SAK 0x20 advertises ISO-DEP. Unused FeliCa,
     * NFCID3, general bytes and historical bytes are zero-initialized. */
    const uint8_t target[37] = {0x05, 0x04, 0x00, 0x12, 0x34, 0x56, 0x20};
    const int64_t deadline = esp_timer_get_time() + TAG_WINDOW_US;
    for (;;) {
        esp_err_t err = begin_command(0x8c, target, sizeof(target));
        if (err != ESP_OK) return err;
        if (on_ready) {
            on_ready();
            on_ready = NULL;
        }
        uint8_t data[FRAME_DATA_MAX];
        size_t size = sizeof(data);
        err = response(0x8c, data, &size, deadline);
        if (err != ESP_OK) return err;
        if (size < 1) return ESP_ERR_INVALID_RESPONSE;

        tag_file_t file = FILE_NONE;
        bool selected = false;
        while (esp_timer_get_time() < deadline) {
            size = sizeof(data);
            err = command(0x86, NULL, 0, data, &size, deadline);
            if (err != ESP_OK) return err;
            if (size < 1) return ESP_ERR_INVALID_RESPONSE;
            if (data[0] != 0x00) break; /* Reader left field. Wait again if needed. */
            uint8_t reply[NDEF_FILE_CAPACITY + 2];
            bool read_end;
            size_t reply_size = tag_reply(&file, &selected, data + 1, size - 1,
                                          ndef, ndef_size, reply, &read_end);
            size = 1; /* TgSetData response is one status byte. */
            err = command(0x8e, reply, reply_size, data, &size, deadline);
            if (err != ESP_OK) return err;
            if (size < 1) return ESP_ERR_INVALID_RESPONSE;
            if (data[0] != 0x00) break;
            if (read_end) *read = true;
        }
        if (*read) return ESP_OK;
        if (esp_timer_get_time() >= deadline) return ESP_ERR_TIMEOUT;
        vTaskDelay(1);
    }
}

esp_err_t pn532_share_url(const char *url, void (*on_ready)(void), bool *read)
{
    if (!read) return ESP_ERR_INVALID_ARG;
    *read = false;
    if (!url || strncmp(url, "https://", 8) != 0) return ESP_ERR_INVALID_ARG;
    const char *suffix = url + 8;
    const size_t suffix_length = strlen(suffix);
    /* Type 4 NLEN, then a short NDEF URI record with prefix code 0x04 (https://). */
    uint8_t ndef[NDEF_FILE_CAPACITY];
    if (!suffix_length || suffix_length > sizeof(ndef) - 7) return ESP_ERR_INVALID_ARG;
    ndef[0] = 0;
    ndef[1] = suffix_length + 5;
    ndef[2] = 0xd1;
    ndef[3] = 1;
    ndef[4] = suffix_length + 1;
    ndef[5] = 'U';
    ndef[6] = 0x04;
    memcpy(ndef + 7, suffix, suffix_length);

    const uart_config_t uart = {
        .baud_rate = 115200, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(BOARD_NFC_UART, 512, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;
    err = uart_param_config(BOARD_NFC_UART, &uart);
    if (err == ESP_OK) {
        err = uart_set_pin(BOARD_NFC_UART, BOARD_NFC_TX_GPIO, BOARD_NFC_RX_GPIO,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (err != ESP_OK) {
        uart_driver_delete(BOARD_NFC_UART);
        return err;
    }

    const uint8_t sam[] = {0x01, 0x14, 0x00}; /* Normal mode, no IRQ wire. */
    uint8_t reply[8];
    size_t size = sizeof(reply);
    uart_flush_input(BOARD_NFC_UART);
    err = command(0x14, sam, sizeof(sam), reply, &size, esp_timer_get_time() + 500000);
    if (err == ESP_OK) {
        size = sizeof(reply);
        err = command(0x02, NULL, 0, reply, &size, esp_timer_get_time() + 500000);
        if (err == ESP_OK && (size < 4 || reply[0] != 0x32)) {
            ESP_LOGW(TAG, "Unexpected firmware version response (%u bytes)", (unsigned)size);
            err = ESP_ERR_INVALID_RESPONSE;
        } else if (err == ESP_OK) {
            ESP_LOGI(TAG, "UART online, PN532 firmware %u.%u", reply[1], reply[2]);
        }
    }
    if (err == ESP_OK) {
        err = emulate(ndef, suffix_length + 7, on_ready, read);
        abort_command();
    }

    uart_driver_delete(BOARD_NFC_UART);
    return *read ? ESP_OK : err;
}
