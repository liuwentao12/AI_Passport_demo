#include "touch.h"
#include <stddef.h>

bool board_touch_is_available(void)
{
    return false;
}

bool board_touch_read(board_touch_sample_t *sample)
{
    if (sample != NULL) {
        *sample = (board_touch_sample_t){0};
    }
    return false; /* No guessed controller, raw ranges or calibration. */
}
