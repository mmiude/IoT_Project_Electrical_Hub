#include "home_screen.h"
#include "screen_common.h"
#include "screen_manager.h"

// TODO: 
// for now it's testing the layout/navigation :)

static void add_btn_cb(lv_event_t *e)
{
    screen_manager_show(ScreenId::DEVICE_MANAGEMENT);
}

static void settings_btn_cb(lv_event_t *e)
{
    screen_manager_show(ScreenId::SETTINGS);
}

lv_obj_t *create_home_screen(void)
{
    lv_obj_t *header;
    lv_obj_t *scr = create_screen_with_header("Home", nullptr, &header);

    lv_obj_t *add_btn = create_icon_button(header, LV_SYMBOL_PLUS, 0x333333);
    lv_obj_align(add_btn, LV_ALIGN_RIGHT_MID, -44, 0);
    lv_obj_add_event_cb(add_btn, add_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *settings_btn = create_icon_button(header, LV_SYMBOL_SETTINGS, 0x333333);
    lv_obj_align(settings_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(settings_btn, settings_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *placeholder = lv_label_create(scr);
    lv_label_set_text(placeholder, "Price / usage / device list\n(placeholder)");
    lv_obj_set_style_text_color(placeholder, lv_color_hex(0x999999), 0);
    lv_obj_center(placeholder);

    return scr;
}