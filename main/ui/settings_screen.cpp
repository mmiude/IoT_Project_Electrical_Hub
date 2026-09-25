#include "settings_screen.h"
#include "screen_common.h"
#include "screen_manager.h"

#include <cstdio>

namespace {

constexpr uint32_t CARD_BG = 0x262626;

// sliders work in tenths of a c/kWh (into-300) should be changed maybe?

constexpr int32_t SLIDER_MIN = 0;
constexpr int32_t SLIDER_MAX = 300;

class SettingsScreen : public UiModelListener {
public:
    lv_obj_t *build(UiModel &model);

private:
    void on_thresholds_changed(float low, float med) override;

    lv_obj_t *build_slider_block(lv_obj_t *parent, const char *title, uint32_t color, lv_obj_t **out_value_label,
                                  lv_event_cb_t changed_cb, lv_event_cb_t released_cb);
    void refresh_value_label(lv_obj_t *label, float value);

    void open_wifi_popup();
    void close_wifi_popup();
    void save_wifi_popup();

    static void back_btn_cb(lv_event_t *e);
    static void network_row_cb(lv_event_t *e);
    static void low_slider_changed_cb(lv_event_t *e);
    static void low_slider_released_cb(lv_event_t *e);
    static void med_slider_changed_cb(lv_event_t *e);
    static void med_slider_released_cb(lv_event_t *e);
    static void wifi_save_btn_cb(lv_event_t *e);
    static void wifi_cancel_btn_cb(lv_event_t *e);
    static void wifi_field_focused_cb(lv_event_t *e);

    UiModel *model{};
    lv_obj_t *low_slider{};
    lv_obj_t *low_value_label{};
    lv_obj_t *med_slider{};
    lv_obj_t *med_value_label{};
    lv_obj_t *wifi_text{};

    // wifi popup, only one open at a time, same type as device management's edit popup
    lv_obj_t *popup_overlay{};
    lv_obj_t *popup_ssid_ta{};
    lv_obj_t *popup_password_ta{};
    lv_obj_t *popup_keyboard{};
};

SettingsScreen *g_screen = nullptr;

void SettingsScreen::back_btn_cb(lv_event_t *)
{
    screen_manager_show(ScreenId::HOME);
}

void SettingsScreen::network_row_cb(lv_event_t *)
{
    if (g_screen) g_screen->open_wifi_popup();
}

void SettingsScreen::wifi_cancel_btn_cb(lv_event_t *)
{
    if (g_screen) g_screen->close_wifi_popup();
}

void SettingsScreen::wifi_save_btn_cb(lv_event_t *)
{
    if (g_screen) g_screen->save_wifi_popup();
}

// keyboard doesn't automatically follow focus -> rebinda it to whichever field was just tapped
void SettingsScreen::wifi_field_focused_cb(lv_event_t *e)
{
    if (!g_screen || !g_screen->popup_keyboard) return;
    auto *ta = static_cast<lv_obj_t *>(lv_event_get_target(e));
    lv_keyboard_set_textarea(g_screen->popup_keyboard, ta);
}

// drag (does not save)
void SettingsScreen::low_slider_changed_cb(lv_event_t *e)
{
    if (!g_screen) return;
    auto *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
    float value = lv_slider_get_value(slider) / 10.0f;
    g_screen->refresh_value_label(g_screen->low_value_label, value);
}

// only send the final value once the user lets go
void SettingsScreen::low_slider_released_cb(lv_event_t *e)
{
    if (!g_screen) return;
    auto *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
    float value = lv_slider_get_value(slider) / 10.0f;
    g_screen->model->set_threshold_low(value);
}

// same with next two
void SettingsScreen::med_slider_changed_cb(lv_event_t *e)
{
    if (!g_screen) return;
    auto *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
    float value = lv_slider_get_value(slider) / 10.0f;
    g_screen->refresh_value_label(g_screen->med_value_label, value);
}

void SettingsScreen::med_slider_released_cb(lv_event_t *e)
{
    if (!g_screen) return;
    auto *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
    float value = lv_slider_get_value(slider) / 10.0f;
    g_screen->model->set_threshold_medium(value);
}

void SettingsScreen::refresh_value_label(lv_obj_t *label, float value)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f c/kWh", value);
    lv_label_set_text(label, buf);
}

lv_obj_t *SettingsScreen::build_slider_block(lv_obj_t *parent, const char *title, uint32_t color, lv_obj_t **out_value_label,
                                              lv_event_cb_t changed_cb, lv_event_cb_t released_cb)
{
    lv_obj_t *block = lv_obj_create(parent);
    lv_obj_set_size(block, 416, 46);
    lv_obj_set_style_bg_opa(block, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(block, 0, 0);
    lv_obj_set_style_pad_all(block, 0, 0);
    lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(block, 4, 0);

    lv_obj_t *header = lv_obj_create(block);
    lv_obj_set_size(header, 416, 16);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x999999), 0);

    lv_obj_t *value_label = lv_label_create(header);
    lv_obj_set_style_text_color(value_label, lv_color_hex(color), 0);
    if (out_value_label) *out_value_label = value_label;

    lv_obj_t *slider = lv_slider_create(block);
    lv_obj_set_size(slider, 416, 16);
    lv_slider_set_range(slider, SLIDER_MIN, SLIDER_MAX);
    lv_obj_set_style_bg_color(slider, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(color), LV_PART_KNOB);
    lv_obj_add_event_cb(slider, changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, released_cb, LV_EVENT_RELEASED, NULL);

    return slider;
}

void SettingsScreen::on_thresholds_changed(float low, float med)
{
    lv_slider_set_value(low_slider, (int32_t)(low * 10.0f), LV_ANIM_OFF);
    refresh_value_label(low_value_label, low);
    lv_slider_set_value(med_slider, (int32_t)(med * 10.0f), LV_ANIM_OFF);
    refresh_value_label(med_value_label, med);
}

void SettingsScreen::open_wifi_popup()
{
    if (popup_overlay) close_wifi_popup();

    popup_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(popup_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(popup_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(popup_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(popup_overlay, 0, 0);
    lv_obj_set_style_radius(popup_overlay, 0, 0);
    lv_obj_set_style_pad_all(popup_overlay, 0, 0);
    lv_obj_clear_flag(popup_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = lv_obj_create(popup_overlay);
    lv_obj_set_size(card, 440, 176);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x262626), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "Wi-Fi Setup");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    popup_ssid_ta = lv_textarea_create(card);
    lv_obj_set_size(popup_ssid_ta, 416, 36);
    lv_textarea_set_one_line(popup_ssid_ta, true);
    lv_textarea_set_max_length(popup_ssid_ta, 31); // wifi_credentials_t::ssid 32 bytes
    lv_textarea_set_placeholder_text(popup_ssid_ta, "Wi-Fi name (SSID)");
    lv_obj_add_event_cb(popup_ssid_ta, wifi_field_focused_cb, LV_EVENT_FOCUSED, NULL);

    popup_password_ta = lv_textarea_create(card);
    lv_obj_set_size(popup_password_ta, 416, 36);
    lv_textarea_set_one_line(popup_password_ta, true);
    lv_textarea_set_password_mode(popup_password_ta, true);
    lv_textarea_set_max_length(popup_password_ta, 63); // wifi_credentials_t::password 64 bytes
    lv_textarea_set_placeholder_text(popup_password_ta, "Password");
    lv_obj_add_event_cb(popup_password_ta, wifi_field_focused_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *btn_row = lv_obj_create(card);
    lv_obj_set_size(btn_row, 416, 40);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *cancel_btn = lv_button_create(btn_row);
    lv_obj_set_size(cancel_btn, 200, 40);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x333333), 0);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(cancel_btn, wifi_cancel_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_btn = lv_button_create(btn_row);
    lv_obj_set_size(save_btn, 200, 40);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_t *save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, "Save");
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(save_btn, wifi_save_btn_cb, LV_EVENT_CLICKED, NULL);

    popup_keyboard = lv_keyboard_create(popup_overlay);
    lv_obj_set_size(popup_keyboard, LV_PCT(100), 118);
    lv_obj_align(popup_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(popup_keyboard, popup_ssid_ta); // starts on the ssid field
}

void SettingsScreen::save_wifi_popup()
{
    if (!popup_overlay || !popup_ssid_ta || !popup_password_ta) return;

    const char *ssid = lv_textarea_get_text(popup_ssid_ta);
    const char *password = lv_textarea_get_text(popup_password_ta);
    if (ssid == nullptr || ssid[0] == '\0' || password == nullptr || password[0] == '\0') return; // keep popup open, need both

    model->set_wifi_credentials(ssid, password);
    if (wifi_text) lv_label_set_text(wifi_text, "Wi-Fi credentials saved!");
    close_wifi_popup();
}

void SettingsScreen::close_wifi_popup()
{
    if (popup_overlay) {
        lv_obj_delete(popup_overlay);
        popup_overlay = nullptr;
    }
    popup_ssid_ta = nullptr;
    popup_password_ta = nullptr;
    popup_keyboard = nullptr;
}

lv_obj_t *SettingsScreen::build(UiModel &m)
{
    model = &m;
    g_screen = this;

    lv_obj_t *header;
    lv_obj_t *scr = create_screen_with_header("System Settings", nullptr, &header);

    lv_obj_t *back_btn = create_icon_button(header, LV_SYMBOL_LEFT, 0x4CAF50);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    // here's a possible idea for this,
    // ssid/pass popup -> saves via UiModel::set_wifi_credentials -> hubcontroller -> nsv
    // we need to boot up again tho...

    lv_obj_t *network_label = lv_label_create(scr);
    lv_label_set_text(network_label, "Network");
    lv_obj_set_style_text_color(network_label, lv_color_hex(0x999999), 0);
    lv_obj_align(network_label, LV_ALIGN_TOP_LEFT, 14, 74);

    lv_obj_t *network_row = lv_obj_create(scr);
    lv_obj_set_size(network_row, 460, 54);
    lv_obj_align(network_row, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_set_style_bg_color(network_row, lv_color_hex(CARD_BG), 0);
    lv_obj_set_style_radius(network_row, 12, 0);
    lv_obj_set_style_border_width(network_row, 0, 0);
    lv_obj_set_style_pad_all(network_row, 12, 0);
    lv_obj_clear_flag(network_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(network_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(network_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(network_row, 10, 0);
    lv_obj_add_event_cb(network_row, network_row_cb, LV_EVENT_CLICKED, NULL);

    // grey dot -> unknown/not-wired status, not "known offline"
    lv_obj_t *status_dot = lv_obj_create(network_row);
    lv_obj_set_size(status_dot, 10, 10);
    lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_dot, lv_color_hex(0x666666), 0);
    lv_obj_set_style_border_width(status_dot, 0, 0);
    lv_obj_clear_flag(status_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(status_dot, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *wifi_icon = lv_label_create(network_row);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xFFFFFF), 0);

    wifi_text = lv_label_create(network_row);
    lv_label_set_text(wifi_text, "Wi-Fi - tap to set up");
    lv_obj_set_style_text_color(wifi_text, lv_color_hex(0x999999), 0);

    // ---- Price threshold section ---
    lv_obj_t *threshold_label = lv_label_create(scr);
    lv_label_set_text(threshold_label, "Price Threshold");
    lv_obj_set_style_text_color(threshold_label, lv_color_hex(0x999999), 0);
    lv_obj_align(threshold_label, LV_ALIGN_TOP_LEFT, 14, 158);

    lv_obj_t *threshold_card = lv_obj_create(scr);
    lv_obj_set_size(threshold_card, 460, 116);
    lv_obj_align(threshold_card, LV_ALIGN_TOP_MID, 0, 180);
    lv_obj_set_style_bg_color(threshold_card, lv_color_hex(CARD_BG), 0);
    lv_obj_set_style_radius(threshold_card, 12, 0);
    lv_obj_set_style_border_width(threshold_card, 0, 0);
    lv_obj_set_style_pad_all(threshold_card, 10, 0);
    lv_obj_clear_flag(threshold_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(threshold_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(threshold_card, 8, 0);

    low_slider = build_slider_block(threshold_card, "Low priority cutoff", priority_color(1), &low_value_label,
                                     low_slider_changed_cb, low_slider_released_cb);
    med_slider = build_slider_block(threshold_card, "Medium priority cutoff", priority_color(2), &med_value_label,
                                     med_slider_changed_cb, med_slider_released_cb);

    on_thresholds_changed(model->threshold_low(), model->threshold_medium());

    model->add_listener(this);
    return scr;
}

} // namespace

lv_obj_t *create_settings_screen(UiModel &model)
{
    static SettingsScreen screen;
    return screen.build(model);
}
