#include "screen_common.h"

lv_obj_t *create_screen_with_header(const char *title, const char *subtitle, lv_obj_t **out_header, lv_obj_t **out_subtitle)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A1A), 0);

    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, 460, 54);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x262626), 0);
    lv_obj_set_style_radius(header, 12, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 8, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 4, 0);

    if (subtitle) {
        lv_obj_t *sub_label = lv_label_create(header);
        lv_label_set_text(sub_label, subtitle);
        lv_obj_set_style_text_color(sub_label, lv_color_hex(0x999999), 0);
        lv_obj_align(sub_label, LV_ALIGN_BOTTOM_LEFT, 4, 0);
        if (out_subtitle) *out_subtitle = sub_label;
    } else if (out_subtitle) {
        *out_subtitle = nullptr;
    }

    if (out_header) {
        *out_header = header;
    }
    return scr;
}

lv_obj_t *create_icon_button(lv_obj_t *parent, const char *symbol, uint32_t bg_color)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 34, 34);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_color), 0);

    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(icon);

    return btn;
}

const char *priority_label(int priority)
{
    switch (priority) {
        case 1: return "Low";
        case 2: return "Medium";
        default: return "Critical";
    }
}

uint32_t priority_color(int priority)
{
    switch (priority) {
        case 1: return 0x2979FF;  // blue LOW
        case 2: return 0xC79A1E;  // amber? MED
        default: return 0xE53935; // red CRITICAL
    }
}

lv_obj_t *create_priority_tag(lv_obj_t *parent, int priority)
{
    lv_obj_t *tag = lv_obj_create(parent);
    lv_obj_set_size(tag, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(tag, 8, 0);
    lv_obj_set_style_border_width(tag, 0, 0);
    lv_obj_set_style_pad_left(tag, 10, 0);
    lv_obj_set_style_pad_right(tag, 10, 0);
    lv_obj_set_style_pad_top(tag, 4, 0);
    lv_obj_set_style_pad_bottom(tag, 4, 0);
    lv_obj_clear_flag(tag, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tag, LV_OBJ_FLAG_CLICKABLE); // taps should fall through to the row behind it

    lv_obj_t *label = lv_label_create(tag);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(label);

    set_priority_tag(tag, priority);
    return tag;
}

void set_priority_tag(lv_obj_t *tag, int priority)
{
    lv_obj_set_style_bg_color(tag, lv_color_hex(priority_color(priority)), 0);
    lv_obj_t *label = lv_obj_get_child(tag, 0);
    lv_label_set_text(label, priority_label(priority));
}