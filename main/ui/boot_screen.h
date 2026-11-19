#pragma once

#include <stdbool.h>

/* UI task only. prepare_home builds the underlying UI and returns an optional
 * note. Its returned string must have static storage duration. */
bool boot_screen_start(const char *(*prepare_home)(void));
void boot_screen_set_status(const char *status);
bool boot_screen_active(void);
