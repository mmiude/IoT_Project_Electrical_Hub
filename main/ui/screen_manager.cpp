#include "screen_manager.h"
#include "home_screen.h"
#include "device_management_screen.h"
#include "settings_screen.h"
#include "lvgl.h"

static lv_obj_t *screens[3];

void screen_manager_init(void)
{
    screens[(int)ScreenId::HOME] = create_home_screen();
    screens[(int)ScreenId::DEVICE_MANAGEMENT] = create_device_management_screen();
    screens[(int)ScreenId::SETTINGS] = create_settings_screen();
    screen_manager_show(ScreenId::HOME);
}

void screen_manager_show(ScreenId id)
{
    lv_screen_load(screens[(int)id]);
}