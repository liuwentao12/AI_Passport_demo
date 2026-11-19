#include <assert.h>
#include <string.h>
#include "app/identity.h"

int main(void)
{
    identity_t data = *identity_demo();
    assert(identity_is_valid(&data));
    assert(!identity_is_valid(NULL));
    data.name[0] = '\0';
    assert(!identity_is_valid(&data));
    data = *identity_demo();
    memset(data.name, 'A', sizeof(data.name));
    assert(!identity_is_valid(&data));
    data.name[sizeof(data.name) - 1] = '\0';
    assert(identity_is_valid(&data));
    memset(data.description, 'x', sizeof(data.description));
    assert(!identity_is_valid(&data));
    data.description[sizeof(data.description) - 1] = '\0';
    assert(identity_is_valid(&data));
    return 0;
}
