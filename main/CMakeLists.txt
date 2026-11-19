idf_component_register(
    SRCS "main.c" "microphone_test.c" "touchscreen_test.c"
         "app/app.c" "app/identity.c" "app/voice_chat.c" "app/siliconflow.c"
         "board/display.c" "board/touch.c" "board/touch_transform.c" "board/keys.c" "board/buzzer.c"
         "ui/port/touch_calibration.c"
         "ui/port/lvgl_port.c" "ui/ui.c" "ui/ui_theme.c"
         "ui/pages/identity_page.c" "ui/pages/details_page.c"
         "ui/pages/wooden_fish_page.c" "ui/pages/wooden_fish_art.c"
         "board/microphone.c" "ui/pages/voice_chat_page.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_i2s esp_wifi esp_event esp_netif nvs_flash esp_http_server
                  esp_driver_gpio esp_driver_spi esp_driver_ledc esp_lcd esp_timer lvgl
                  esp_http_client mbedtls
)
