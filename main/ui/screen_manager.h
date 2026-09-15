#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

enum class ScreenId {
    HOME,
    DEVICE_MANAGEMENT,
    SETTINGS,
};

// creates all screens and loads the first one
void screen_manager_init(void);

// switches the active screen
void screen_manager_show(ScreenId id);

#endif // SCREEN_MANAGER_H