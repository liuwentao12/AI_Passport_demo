#pragma once

#include <stdbool.h>

/* All calls run in the UI task. Start after boot and touch calibration. */
void idle_lock_start(void (*set_dimmed)(bool));
void idle_lock_poll(void);

/* Records input activity. Returns true when this input only wakes the screen. */
bool idle_lock_handle_input(void);
