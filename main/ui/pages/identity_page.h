#pragma once

#include "lvgl.h"
#include "app/identity.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *name;
    lv_obj_t *initials;
    lv_obj_t *role;
    lv_obj_t *number;
    lv_obj_t *location;
} identity_page_t;

void identity_page_create(identity_page_t *page, lv_obj_t *parent);
void identity_page_update(identity_page_t *page, const identity_t *data);
