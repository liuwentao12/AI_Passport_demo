#pragma once

#include "esp_err.h"

/* Called once by the UI task. Display errors are fatal; returns touch setup status. */
esp_err_t ui_port_init(void);
