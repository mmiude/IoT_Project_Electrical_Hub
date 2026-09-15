#include "device_management_screen.h"
#include "screen_common.h"
#include "screen_manager.h"

// TODO: real device list (id, power, priority) driven by uiQueue messages + "add zigbee device"
// for now it's testing the layout/navigation :)

static void back_btn_cb(lv_event_t *e)
{
    screen_manager_show(ScreenId::HOME);
}

lv_obj_t *create_device_management_screen(void)
{
    lv_obj_t *header;
    lv_obj_t *scr = create_screen_with_header("Device Management", "0 Connected", &header);

    lv_obj_t *back_btn = create_icon_button(header, LV_SYMBOL_LEFT, 0x4CAF50);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *placeholder = lv_label_create(scr);
    lv_label_set_text(placeholder, "Device list + add device\n(placeholder)");
    lv_obj_set_style_text_color(placeholder, lv_color_hex(0x999999), 0);
    lv_obj_center(placeholder);

    return scr;
}