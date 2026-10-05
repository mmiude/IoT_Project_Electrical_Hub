#ifndef SCREEN_COMMON_H
#define SCREEN_COMMON_H

#include "lvgl.h"

// made this o act as a "QOL" tool

lv_obj_t *create_screen_with_header(const char *title, const char *subtitle, lv_obj_t **out_header, lv_obj_t **out_subtitle = nullptr);

lv_obj_t *create_icon_button(lv_obj_t *parent, const char *symbol, uint32_t bg_color, int32_t size = 34);

// 1 LOW, 2 MEDIUM, 0 CRITICAL
const char *priority_label(int priority);
uint32_t priority_color(int priority);

lv_obj_t *create_priority_tag(lv_obj_t *parent, int priority);

// updates a pill when prio changes
void set_priority_tag(lv_obj_t *tag, int priority);

#endif // SCREEN_COMMON_H