#include <assert.h>
#include "board/touch_transform.h"

int main(void)
{
    /* Swapped axes plus reversed Y: no panel-specific mirror assumptions. */
    touch_point_t raw[3] = {{3700, 400}, {3700, 3500}, {400, 400}};
    const touch_point_t target[3] = {{24, 24}, {215, 24}, {24, 295}};
    touch_transform_t transform;
    assert(touch_transform_fit(&transform, raw, target));
    touch_point_t result;
    for (int i = 0; i < 3; ++i) {
        assert(touch_transform_apply(&transform, raw[i], &result));
        assert(result.x == target[i].x && result.y == target[i].y);
    }
    assert(touch_transform_apply(&transform, (touch_point_t){2050, 1950}, &result));
    assert(result.x >= 119 && result.x <= 120);
    assert(result.y >= 159 && result.y <= 160);
    raw[2] = raw[1];
    assert(!touch_transform_fit(&transform, raw, target));
    raw[2] = (touch_point_t){5000, 400};
    assert(!touch_transform_fit(&transform, raw, target));
    assert(!touch_transform_fit(NULL, raw, target));
    return 0;
}
