#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* Blocking Type 4 read-only tag session. Call from one background task. */
esp_err_t pn532_share_url(const char *url, void (*on_ready)(void), bool *read);
