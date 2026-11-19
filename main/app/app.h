#pragma once

#include "esp_err.h"
#include "identity.h"

/* Start once from app_main. Startup failures are fatal. */
void app_start(void);
/* Task context only, after app_start returns. Copies the value, never blocks.
 * Multiple pending updates collapse to the latest value. No LVGL access here. */
esp_err_t app_set_identity(const identity_t *identity);
