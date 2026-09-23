#ifndef SCREEN_COMMON_H
#define SCREEN_COMMON_H

#include "lvgl.h"

// made this o act as a "QOL" tool

// creates the shared dark-background screen + header bar with a title
// returns the screen object, out_header gives the caller a place to add
// nav buttons (top-right icon button, etc). out_subtitle (optional) hands back the
// subtitle label so the caller can update it later (e.g. a live "N Connected" count)
lv_obj_t *create_screen_with_header(const char *title, const char *subtitle, lv_obj_t **out_header, lv_obj_t **out_subtitle = nullptr);

// small round icon button: + / gear / back-arrow buttons.
lv_obj_t *create_icon_button(lv_obj_t *parent, const char *symbol, uint32_t bg_color);

// priority ints match HubController::threshold_allows_opening(): 1 = cut off first (uses LOW prio threshold)
// , 2 = cut off later (uses MED prio treshold), anything else ((0) CRITICAL) is never turned off automatically
// included here so every screen matches on the same label/color = prio
const char *priority_label(int priority);
uint32_t priority_color(int priority);

// small colored pill showing a device's priority (see priority_label/priority_color)
lv_obj_t *create_priority_tag(lv_obj_t *parent, int priority);
// updates a pill created by create_priority_tag in place, when the device's priority changes
void set_priority_tag(lv_obj_t *tag, int priority);

#endif // SCREEN_COMMON_H