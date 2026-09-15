#include "settings_screen.h"
#include "screen_common.h"
#include "screen_manager.h"

// TODO: price threshold slider, WiFi/network settings, (automation log)
// for now it's testing the layout/navigation :)

static void back_btn_cb(lv_event_t *e)
{
    screen_manager_show(ScreenId::HOME);
}

lv_obj_t *create_settings_screen(void)
{
    lv_obj_t *header;
    lv_obj_t *scr = create_screen_with_header("System Settings", nullptr, &header);

    lv_obj_t *back_btn = create_icon_button(header, LV_SYMBOL_LEFT, 0x4CAF50);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *placeholder = lv_label_create(scr);
    lv_label_set_text(placeholder, "Threshold / WiFi / log\n(placeholder)");
    lv_obj_set_style_text_color(placeholder, lv_color_hex(0x999999), 0);
    lv_obj_center(placeholder);

    return scr;
}