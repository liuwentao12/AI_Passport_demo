#include "touchscreen_test.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"

#define LCD_HOST        SPI2_HOST

#define LCD_PIN_SCK     18
#define LCD_PIN_MOSI    23
#define LCD_PIN_MISO    19
#define LCD_PIN_CS      16
#define LCD_PIN_DC      21
#define LCD_PIN_RST     22
#define TOUCH_PIN_CS    GPIO_NUM_17
static SemaphoreHandle_t lcd_done_sem;
// 竖屏方向：240 x 320
#define LCD_WIDTH       240
#define LCD_HEIGHT      320
#define BLOCK_ROWS      20

static const char *TAG = "touchscreen-test";

// SPI DMA 传输结束回调
static bool lcd_trans_done(
    esp_lcd_panel_io_handle_t io,
    esp_lcd_panel_io_event_data_t *edata,
    void *user_ctx)
{
    BaseType_t woken = pdFALSE;

    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_ctx, &woken);

    return woken == pdTRUE;
}

void touchscreen_test_start(void)
{
    ESP_LOGI(TAG, "Initializing ILI9341");

    // 1. 禁用触摸芯片，防止它干扰 SPI 总线
    ESP_ERROR_CHECK(gpio_reset_pin(TOUCH_PIN_CS));
    ESP_ERROR_CHECK(gpio_set_direction(
        TOUCH_PIN_CS, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(TOUCH_PIN_CS, 1));

    lcd_done_sem = xSemaphoreCreateBinary();
    assert(lcd_done_sem != NULL);

    // 2. 初始化 SPI 总线
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = LCD_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz =
            LCD_WIDTH * BLOCK_ROWS * 2,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(
        LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. 创建 LCD SPI 设备接口
    esp_lcd_panel_io_handle_t io_handle = NULL;

    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_PIN_CS,
        .dc_gpio_num = LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = 10 * 1000 * 1000,
        .trans_queue_depth = 4,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .on_color_trans_done = lcd_trans_done,
        .user_ctx = lcd_done_sem,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)LCD_HOST,
        &io_config, &io_handle));

    // 4. 创建 ILI9341 面板驱动
    esp_lcd_panel_handle_t panel = NULL;

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(
        io_handle, &panel_config, &panel));

    // 5. 复位、初始化、打开显示
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(
        esp_lcd_panel_disp_on_off(panel, true));

    ESP_LOGI(TAG, "LCD initialized");

    // 6. 分配 20 行 RGB565 DMA 缓冲区
    size_t buf_size =
        LCD_WIDTH * BLOCK_ROWS * sizeof(uint16_t);

    uint16_t *buf = heap_caps_malloc(
        buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    assert(buf != NULL);

    // 7. 八种测试颜色
    const uint16_t colors[8] = {
        0xF800, // Red
        0x07E0, // Green
        0x001F, // Blue
        0xFFFF, // White
        0x0000, // Black
        0xFFE0, // Yellow
        0x07FF, // Cyan
        0xF81F, // Magenta
    };

    for (int band = 0; band < 8; band++) {
        // 转换像素的字节顺序供 SPI 发送
        uint16_t pixel = __builtin_bswap16(
            colors[band]);

        for (int i = 0;
             i < LCD_WIDTH * BLOCK_ROWS; i++) {
            buf[i] = pixel;
        }

        for (int y = band * 40;
             y < (band + 1) * 40;
             y += BLOCK_ROWS) {

            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
                panel, 0, y,
                LCD_WIDTH, y + BLOCK_ROWS, buf));

            // 等待本次 DMA 传输完成后复用缓冲区
            xSemaphoreTake(
                lcd_done_sem, portMAX_DELAY);
        }
    }

    ESP_LOGI(TAG, "Color test complete");
    free(buf);

}
