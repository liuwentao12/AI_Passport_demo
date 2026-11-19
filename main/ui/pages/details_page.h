#pragma once

#include "lvgl.h"
#include "app/identity.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *name;
    lv_obj_t *role;
    lv_obj_t *number;
    lv_obj_t *location;
    lv_obj_t *issued;
    lv_obj_t *description;
} details_page_t;

void details_page_create(details_page_t *page, lv_obj_t *parent);
void details_page_update(details_page_t *page, const identity_t *data);
