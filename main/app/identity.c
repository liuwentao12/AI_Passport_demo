#include "identity.h"

#include <string.h>

const identity_t *identity_demo(void)
{
    static const identity_t demo = {
        .name = "Alex Chen",
        .initials = "AC",
        .role = "CREATOR / EXPLORER",
        .number = "AP-2026-0001",
        .location = "Shenzhen, China",
        .issued = "2026.11.19",
        .description = "Building small things.\nExploring big ideas.",
    };
    return &demo;
}

bool identity_is_valid(const identity_t *identity)
{
    if (identity == NULL || identity->name[0] == '\0') {
        return false;
    }
#define TERMINATED(field) (memchr(identity->field, '\0', sizeof(identity->field)) != NULL)
    return TERMINATED(name) && TERMINATED(initials) && TERMINATED(role) &&
           TERMINATED(number) && TERMINATED(location) && TERMINATED(issued) &&
           TERMINATED(description);
#undef TERMINATED
}
