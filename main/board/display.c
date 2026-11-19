#include "display.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_dma_done;
static const char *TAG = "display";
static bool s_backlight_ready;

enum {
    BACKLIGHT_FULL_DUTY = 255,
    BACKLIGHT_DIM_DUTY = 90,
};

static esp_err_t init_backlight(void)
{
    if (BOARD_LCD_BACKLIGHT_GPIO < 0) return ESP_OK;
    /* The buzzer owns timer 0 and channel 0. */
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer");
    const ledc_channel_config_t channel = {
        .gpio_num = BOARD_LCD_BACKLIGHT_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_1,
        .duty = BACKLIGHT_FULL_DUTY,
        .flags.output_invert = BOARD_LCD_BACKLIGHT_ACTIVE_LOW,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "backlight channel");
    s_backlight_ready = true;
    return ESP_OK;
}

esp_err_t board_display_set_dimmed(bool dimmed)
{
    if (!s_backlight_ready) return ESP_OK;
    const uint32_t duty = dimmed ? BACKLIGHT_DIM_DUTY : BACKLIGHT_FULL_DUTY;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty),
                        TAG, "backlight duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

static bool transfer_done(esp_lcd_panel_io_handle_t io,
                          esp_lcd_panel_io_event_data_t *event, void *context)
{
    (void)io;
    (void)event;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &woken);
    return woken == pdTRUE;
}

esp_err_t board_display_init(void)
{
    ESP_RETURN_ON_FALSE(s_dma_done == NULL, ESP_ERR_INVALID_STATE, TAG, "Already initialized");
    /* Keep touch deselected until board_touch_init adds it to the shared bus. */
    ESP_RETURN_ON_ERROR(gpio_reset_pin(BOARD_TOUCH_CS), TAG, "touch CS reset");
    ESP_RETURN_ON_ERROR(gpio_set_direction(BOARD_TOUCH_CS, GPIO_MODE_OUTPUT), TAG, "touch CS direction");
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_TOUCH_CS, 1), TAG, "touch CS inactive");
    s_dma_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_dma_done, ESP_ERR_NO_MEM, TAG, "DMA semaphore");

    const spi_bus_config_t bus = {
        .sclk_io_num = BOARD_LCD_SCK, .mosi_io_num = BOARD_LCD_MOSI,
        .miso_io_num = BOARD_LCD_MISO, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_WIDTH * BOARD_LCD_BUFFER_ROWS * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "SPI init");
    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = BOARD_LCD_CS, .dc_gpio_num = BOARD_LCD_DC,
        .spi_mode = 0, .pclk_hz = BOARD_LCD_PIXEL_CLOCK,
        .trans_queue_depth = 1, .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .on_color_trans_done = transfer_done, .user_ctx = s_dma_done,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_HOST,
                                               &io_config, &io), TAG, "LCD IO");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(io, &panel_config, &s_panel), TAG, "ILI9341");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");
    /* Apply after init, which writes the controller's MADCTL register. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, BOARD_LCD_MIRROR_X,
                                            BOARD_LCD_MIRROR_Y), TAG, "orientation");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display on");
    const esp_err_t backlight = init_backlight();
    if (backlight != ESP_OK) {
        ESP_LOGW(TAG, "Backlight PWM unavailable: %s", esp_err_to_name(backlight));
    }
    return ESP_OK;
}

esp_err_t board_display_draw(int x1, int y1, int x2, int y2, const uint8_t *pixels)
{
    ESP_RETURN_ON_FALSE(s_panel && pixels, ESP_ERR_INVALID_STATE, TAG, "display not ready");
    ESP_RETURN_ON_FALSE(x1 >= 0 && y1 >= 0 && x2 > x1 && y2 > y1 &&
                       x2 <= BOARD_LCD_WIDTH && y2 <= BOARD_LCD_HEIGHT &&
                       (x2 - x1) * (y2 - y1) <= BOARD_LCD_WIDTH * BOARD_LCD_BUFFER_ROWS,
                       ESP_ERR_INVALID_ARG, TAG, "invalid draw area");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2, y2, pixels), TAG, "draw");
    /* Fail fast on a lost interrupt. The caller must not reuse this buffer on error. */
    return xSemaphoreTake(s_dma_done, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
