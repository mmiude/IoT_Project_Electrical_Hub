#ifndef SCREEN_COMMON_H
#define SCREEN_COMMON_H

#include "lvgl.h"

// made this o act as a "QOL" tool 

// creates the shared dark-background screen + header bar with a title
// returns the screen object, out_header gives the caller a place to add
// nav buttons (top-right icon button, etc).
lv_obj_t *create_screen_with_header(const char *title, const char *subtitle, lv_obj_t **out_header);

// small round icon button: + / gear / back-arrow buttons.
lv_obj_t *create_icon_button(lv_obj_t *parent, const char *symbol, uint32_t bg_color);

#endif // SCREEN_COMMON_H