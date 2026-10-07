#ifndef DEVICE_MANAGEMENT_SCREEN_H
#define DEVICE_MANAGEMENT_SCREEN_H

#include "lvgl.h"
#include "ui_model.h"

lv_obj_t *create_device_management_screen(UiModel &model);

// opens the same name/priority popup used when tapping a device row
void device_management_open_edit_popup(uint64_t id);

#endif // DEVICE_MANAGEMENT_SCREEN_H
