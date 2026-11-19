#include "lvgl_port.h"

#include "lvgl.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "board/board_config.h"
#include "board/display.h"
#include "board/touch.h"

static uint32_t tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    uint32_t count = (uint32_t)lv_area_get_width(area) * lv_area_get_height(area);
    lv_draw_sw_rgb565_swap(pixels, count);
    ESP_ERROR_CHECK(board_display_draw(area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels));
    lv_display_flush_ready(display); /* DMA is finished, not merely queued. */
}

static void read_touch(lv_indev_t *input, lv_indev_data_t *data)
{
    (void)input;
    board_touch_sample_t sample = {0};
    data->state = LV_INDEV_STATE_RELEASED;
    if (board_touch_read(&sample) && sample.pressed &&
        sample.x < BOARD_LCD_WIDTH && sample.y < BOARD_LCD_HEIGHT) {
        data->point.x = sample.x;
        data->point.y = sample.y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
}

void ui_port_init(void)
{
    ESP_ERROR_CHECK(board_display_init());
    lv_init();
    lv_tick_set_cb(tick_ms);
    lv_display_t *display = lv_display_create(BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
    ESP_ERROR_CHECK(display ? ESP_OK : ESP_ERR_NO_MEM);
    const size_t bytes = BOARD_LCD_WIDTH * BOARD_LCD_BUFFER_ROWS * sizeof(uint16_t);
    void *buffer = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(buffer ? ESP_OK : ESP_ERR_NO_MEM);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);

    lv_indev_t *input = lv_indev_create();
    ESP_ERROR_CHECK(input ? ESP_OK : ESP_ERR_NO_MEM);
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display);
    lv_indev_set_read_cb(input, read_touch);
}
