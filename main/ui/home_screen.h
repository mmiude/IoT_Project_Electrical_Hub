#ifndef HOME_SCREEN_H
#define HOME_SCREEN_H

#include "lvgl.h"
#include "ui_model.h"

// builds the home screen and registers it as a UiModel listener so it stays live-updated
// call once; the returned screen is kept alive for the app's lifetime (see screen_manager)
lv_obj_t *create_home_screen(UiModel &model);

#endif // HOME_SCREEN_H
