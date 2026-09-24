#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include "lvgl.h"
#include "ui_model.h"

// builds the settings screen and registers it as a UiModel listener so the threshold sliders
// stay live-updated. call once; the returned screen is kept alive for the app's lifetime.
lv_obj_t *create_settings_screen(UiModel &model);

#endif // SETTINGS_SCREEN_H
