#pragma once

#include <stdbool.h>

/* Value type: safe to copy through a queue; strings must be NUL terminated.
 * First version uses ASCII fonts. Add a matching font before using UTF-8. */
typedef struct {
    char name[32];
    char initials[4];
    char role[32];
    char number[32];
    char location[48];
    char issued[16];
    char description[128];
} identity_t;

const identity_t *identity_demo(void);
bool identity_is_valid(const identity_t *identity);
