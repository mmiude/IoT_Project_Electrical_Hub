#ifndef DEVICE_MANAGEMENT_SCREEN_H
#define DEVICE_MANAGEMENT_SCREEN_H

#include "lvgl.h"
#include "ui_model.h"

lv_obj_t *create_device_management_screen(UiModel &model);

// opens the same name/priority popup used when tapping a device row, for other UI components
// (e.g. the "new device detected" prompt) that need to jump straight into naming a device.
// no-op if the device management screen hasn't been created yet or the id is unknown.
void device_management_open_edit_popup(uint64_t id);

#endif // DEVICE_MANAGEMENT_SCREEN_H
