#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "ui_model.h"

enum class ScreenId {
    HOME,
    DEVICE_MANAGEMENT,
    SETTINGS,
};

// creates all screens and loads the first one, model must outlive every screen (it does -> both live for the whole app)
void screen_manager_init(UiModel &model);

// switches the active screen
void screen_manager_show(ScreenId id);

#endif // SCREEN_MANAGER_H