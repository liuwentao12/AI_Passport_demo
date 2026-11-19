#include "touch_transform.h"
#include <math.h>

static bool raw_valid(touch_point_t point)
{
    return point.x >= 0 && point.x <= 4095 && point.y >= 0 && point.y <= 4095;
}

bool touch_transform_fit(touch_transform_t *out, const touch_point_t raw[3],
                         const touch_point_t target[3])
{
    if (!out || !raw || !target) return false;
    for (int i = 0; i < 3; ++i) {
        if (!raw_valid(raw[i])) return false;
    }
    float x0 = raw[0].x, x1 = raw[1].x, x2 = raw[2].x;
    float y0 = raw[0].y, y1 = raw[1].y, y2 = raw[2].y;
    float det = x0 * (y1 - y2) + x1 * (y2 - y0) + x2 * (y0 - y1);
    /* Reject repeated/near-collinear taps and unrealistically tiny ADC spans. */
    if (fabsf(det) < 100000.0f) return false;
    for (int axis = 0; axis < 2; ++axis) {
        float t0 = axis ? target[0].y : target[0].x;
        float t1 = axis ? target[1].y : target[1].x;
        float t2 = axis ? target[2].y : target[2].x;
        float *c = axis ? out->y : out->x;
        c[0] = (t0 * (y1 - y2) + t1 * (y2 - y0) + t2 * (y0 - y1)) / det;
        c[1] = (t0 * (x2 - x1) + t1 * (x0 - x2) + t2 * (x1 - x0)) / det;
        c[2] = (t0 * (x1 * y2 - x2 * y1) + t1 * (x2 * y0 - x0 * y2) +
                t2 * (x0 * y1 - x1 * y0)) / det;
    }
    return true;
}

bool touch_transform_apply(const touch_transform_t *transform, touch_point_t raw,
                           touch_point_t *out)
{
    if (!transform || !out || !raw_valid(raw)) return false;
    float x = transform->x[0] * raw.x + transform->x[1] * raw.y + transform->x[2];
    float y = transform->y[0] * raw.x + transform->y[1] * raw.y + transform->y[2];
    if (!isfinite(x) || !isfinite(y) || fabsf(x) > 32767 || fabsf(y) > 32767) return false;
    out->x = (int32_t)(x + (x >= 0 ? 0.5f : -0.5f));
    out->y = (int32_t)(y + (y >= 0 ? 0.5f : -0.5f));
    return true;
}
