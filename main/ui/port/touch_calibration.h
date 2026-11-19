#pragma once

/* UI task only, after ui_port_init and before ui_init. Shows calibration if
 * needed. No callback recursion: this owns its startup event loop. */
void ui_touch_calibrate(void);
