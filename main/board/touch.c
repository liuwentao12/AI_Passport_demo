#include "touch.h"
#include "board_config.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "xpt2046";
static spi_device_handle_t s_device;
static touch_transform_t s_transform;
static bool s_calibrated;
static bool s_storage_ready;
static bool s_read_failed;
static const touch_point_t s_targets[3] = {
    {24, 24}, {BOARD_LCD_WIDTH - 25, 24}, {24, BOARD_LCD_HEIGHT - 25},
};

typedef struct {
    uint32_t version;
    uint16_t width, height;
    uint32_t orientation;
    touch_point_t raw[3];
} calibration_record_t;

static uint32_t orientation(void)
{
    return (BOARD_LCD_MIRROR_X ? 1U : 0U) | (BOARD_LCD_MIRROR_Y ? 2U : 0U);
}

esp_err_t board_touch_init(void)
{
    if (s_device) return ESP_ERR_INVALID_STATE;
    const spi_device_interface_config_t config = {
        .clock_speed_hz = BOARD_TOUCH_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = BOARD_TOUCH_CS,
        .queue_size = 1,
    };
    esp_err_t err = spi_bus_add_device(BOARD_LCD_HOST, &config, &s_device);
    if (err != ESP_OK) return err;

    /* Do not erase other application data if NVS cannot initialize. */
    err = nvs_flash_init();
    s_storage_ready = err == ESP_OK;
    if (!s_storage_ready) {
        ESP_LOGW(TAG, "NVS unavailable (%s); calibration will be RAM-only", esp_err_to_name(err));
    } else if (!BOARD_TOUCH_FORCE_CALIBRATION) {
        nvs_handle_t storage;
        if (nvs_open("touch", NVS_READONLY, &storage) == ESP_OK) {
            calibration_record_t record = {0};
            size_t size = sizeof(record);
            err = nvs_get_blob(storage, "cal_v1", &record, &size);
            nvs_close(storage);
            if (err == ESP_OK && size == sizeof(record) && record.version == 1 &&
                record.width == BOARD_LCD_WIDTH && record.height == BOARD_LCD_HEIGHT &&
                record.orientation == orientation()) {
                s_calibrated = touch_transform_fit(&s_transform, record.raw, s_targets);
            }
        }
    }
    ESP_LOGI(TAG, "SPI touch initialized, CS=%d, PEN unused, calibration %s",
             BOARD_TOUCH_CS, s_calibrated ? "loaded" : "required");
    return ESP_OK;
}

/* 8 command clocks + one null bit + 12 result bits + 3 trailing clocks.
 * PD1:PD0=00: differential conversion, power down after each sample.
 * Repeat after channel switching to allow the resistive panel to settle. */
static esp_err_t read_adc(uint8_t command, uint16_t *value)
{
    for (int i = 0; i < 2; ++i) {
        spi_transaction_t transaction = {
            .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
            .length = 24,
            .tx_data = {command, 0, 0},
        };
        esp_err_t err = spi_device_polling_transmit(s_device, &transaction);
        if (err != ESP_OK) {
            if (!s_read_failed) ESP_LOGE(TAG, "SPI sample failed: %s", esp_err_to_name(err));
            s_read_failed = true;
            return err;
        }
        *value = (((uint16_t)transaction.rx_data[1] << 8) | transaction.rx_data[2]) >> 3;
        *value &= 0x0FFF;
    }
    return ESP_OK;
}

static bool pressure(uint16_t z1, uint16_t z2)
{
    /* Reject floating/all-zero/all-one bus values as well as an untouched panel. */
    return z1 > 0 && z2 > z1 && (4095 - z2 + z1) >= BOARD_TOUCH_PRESSURE;
}

static uint16_t median(uint16_t values[5])
{
    for (int i = 1; i < 5; ++i) {
        uint16_t value = values[i];
        int j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
    return values[2];
}

bool board_touch_read_raw(board_touch_sample_t *sample)
{
    if (!sample) return false;
    *sample = (board_touch_sample_t){0};
    if (!s_device) return false;
    uint16_t z1, z2, x[5], y[5];
    if (read_adc(0xB0, &z1) != ESP_OK || read_adc(0xC0, &z2) != ESP_OK) return false;
    if (!pressure(z1, z2)) {
        s_read_failed = false;
        return true;
    }
    for (int i = 0; i < 5; ++i) {
        if (read_adc(0xD0, &x[i]) != ESP_OK || read_adc(0x90, &y[i]) != ESP_OK) return false;
    }
    /* A release partway through conversion must not become a random tap. */
    if (read_adc(0xB0, &z1) != ESP_OK || read_adc(0xC0, &z2) != ESP_OK) return false;
    s_read_failed = false;
    if (pressure(z1, z2)) {
        sample->x = median(x);
        sample->y = median(y);
        sample->pressed = true;
    }
    return true;
}

bool board_touch_is_available(void)
{
    return s_device != NULL && s_calibrated;
}

bool board_touch_read(board_touch_sample_t *sample)
{
    if (!sample) return false;
    *sample = (board_touch_sample_t){0};
    if (!board_touch_is_available()) return false;
    board_touch_sample_t raw;
    if (!board_touch_read_raw(&raw)) return false;
    if (!raw.pressed) return true;
    touch_point_t pixel;
    if (!touch_transform_apply(&s_transform, (touch_point_t){raw.x, raw.y}, &pixel)) return false;
    if (pixel.x < -20 || pixel.y < -20 ||
        pixel.x >= BOARD_LCD_WIDTH + 20 || pixel.y >= BOARD_LCD_HEIGHT + 20) return true;
    sample->x = pixel.x < 0 ? 0 : pixel.x >= BOARD_LCD_WIDTH ? BOARD_LCD_WIDTH - 1 : pixel.x;
    sample->y = pixel.y < 0 ? 0 : pixel.y >= BOARD_LCD_HEIGHT ? BOARD_LCD_HEIGHT - 1 : pixel.y;
    sample->pressed = true;
    return true;
}

const touch_point_t *board_touch_calibration_targets(void)
{
    return s_targets;
}

esp_err_t board_touch_set_calibration(const touch_point_t raw[3])
{
    touch_transform_t next;
    if (!s_device || !touch_transform_fit(&next, raw, s_targets)) return ESP_ERR_INVALID_ARG;
    s_transform = next;
    s_calibrated = true;
    if (!s_storage_ready) return ESP_ERR_INVALID_STATE;
    calibration_record_t record = {
        .version = 1, .width = BOARD_LCD_WIDTH, .height = BOARD_LCD_HEIGHT,
        .orientation = orientation(), .raw = {raw[0], raw[1], raw[2]},
    };
    nvs_handle_t storage;
    esp_err_t err = nvs_open("touch", NVS_READWRITE, &storage);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(storage, "cal_v1", &record, sizeof(record));
    if (err == ESP_OK) err = nvs_commit(storage);
    nvs_close(storage);
    return err;
}
