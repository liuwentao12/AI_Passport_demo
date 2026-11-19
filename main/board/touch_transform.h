#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct { int32_t x, y; } touch_point_t;
typedef struct { float x[3], y[3]; } touch_transform_t;

/* Pure math, no ESP-IDF or LVGL dependency. raw coordinates must be 12-bit. */
bool touch_transform_fit(touch_transform_t *out, const touch_point_t raw[3],
                         const touch_point_t target[3]);
/* Returns an unclipped screen point; caller owns viewport bounds handling. */
bool touch_transform_apply(const touch_transform_t *transform, touch_point_t raw,
                           touch_point_t *out);
