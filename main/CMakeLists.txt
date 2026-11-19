idf_component_register(
    SRCS "main.c" "microphone_test.c" "touchscreen_test.c"
         "app/app.c" "app/identity.c"
         "board/display.c" "board/touch.c"
         "ui/port/lvgl_port.c" "ui/ui.c" "ui/ui_theme.c"
         "ui/pages/identity_page.c" "ui/pages/details_page.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_i2s esp_wifi esp_event esp_netif nvs_flash esp_http_server
                  esp_driver_gpio esp_driver_spi esp_lcd esp_timer lvgl
)
