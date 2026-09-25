#include "device_management_screen.h"
#include "screen_common.h"
#include "screen_manager.h"

#include <cstdio>
#include <cstdint>
#include <map>
#include "esp_log.h"

// the zigbee join window stays open for 3 minutes after add device has been pressed -> shows listening 
// delete needs to be tapped twice to prevent accidental deletion! 

static constexpr uint32_t JOIN_WINDOW_MS = 180000;

namespace {

class DeviceManagementScreen : public UiModelListener {
public:
    lv_obj_t *build(UiModel &model);
    void open_edit_popup(uint64_t id);

private:
    struct DeviceRow {
        lv_obj_t *container{};
        lv_obj_t *name_label{};
        lv_obj_t *watt_label{};
        lv_obj_t *tag{};
        uint64_t *id_ctx{}; // heap id passed as the row's click event user_data (freed on removal)
    };

    void on_device_added(const UiDevice &dev) override;
    void on_device_updated(const UiDevice &dev) override;
    void on_device_removed(uint64_t id) override;

    void build_row(uint64_t id, const UiDevice &dev);
    void apply_row(DeviceRow &row, const UiDevice &dev);
    void refresh_connected_count();
    void update_empty_state();

    void close_edit_popup();
    void save_edit_popup();
    void select_priority(int priority);
    void set_delete_confirm_mode(bool confirming);

    static void back_btn_cb(lv_event_t *e);
    static void add_device_btn_cb(lv_event_t *e);
    static void add_device_reset_cb(lv_timer_t *timer);
    static void row_click_cb(lv_event_t *e);
    static void save_btn_cb(lv_event_t *e);
    static void cancel_btn_cb(lv_event_t *e);
    static void priority_btn_cb(lv_event_t *e);
    static void delete_btn_cb(lv_event_t *e);
    static void keep_device_btn_cb(lv_event_t *e);
    static void confirm_delete_btn_cb(lv_event_t *e);

    UiModel *model{};
    lv_obj_t *subtitle_label{};
    lv_obj_t *device_list{};
    lv_obj_t *empty_label{};
    lv_obj_t *add_device_label{};

    std::map<uint64_t, DeviceRow> rows;

    // edit popup - only one open at a time
    lv_obj_t *popup_overlay{};
    lv_obj_t *popup_name_ta{};
    lv_obj_t *popup_priority_btns[3]{};
    int popup_priority{0};
    uint64_t popup_device_id{0};

    lv_obj_t *popup_cancel_btn{};
    lv_obj_t *popup_save_btn{};
    lv_obj_t *popup_keep_btn{};
    lv_obj_t *popup_confirm_delete_btn{};
};

DeviceManagementScreen *g_screen = nullptr;

void DeviceManagementScreen::back_btn_cb(lv_event_t *)
{
    screen_manager_show(ScreenId::HOME);
}

void DeviceManagementScreen::add_device_btn_cb(lv_event_t *)
{
    if (!g_screen) return;
    g_screen->model->open_network();
    lv_label_set_text(g_screen->add_device_label, "Listening...");
    lv_timer_t *t = lv_timer_create(add_device_reset_cb, JOIN_WINDOW_MS, g_screen->add_device_label);
    lv_timer_set_repeat_count(t, 1); // one-shot, lvgl deletes it after it fires
}

void DeviceManagementScreen::add_device_reset_cb(lv_timer_t *timer)
{
    auto *label = static_cast<lv_obj_t *>(lv_timer_get_user_data(timer));
    lv_label_set_text(label, "+ Add device");
}

void DeviceManagementScreen::row_click_cb(lv_event_t *e)
{
    if (!g_screen) return;
    auto *id_ptr = static_cast<uint64_t *>(lv_event_get_user_data(e));
    g_screen->open_edit_popup(*id_ptr);
}

void DeviceManagementScreen::save_btn_cb(lv_event_t *)
{
    if (g_screen) g_screen->save_edit_popup();
}

void DeviceManagementScreen::cancel_btn_cb(lv_event_t *)
{
    if (g_screen) g_screen->close_edit_popup();
}

void DeviceManagementScreen::priority_btn_cb(lv_event_t *e)
{
    if (!g_screen) return;
    int priority = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    g_screen->select_priority(priority);
}

void DeviceManagementScreen::delete_btn_cb(lv_event_t *)
{
    if (g_screen) g_screen->set_delete_confirm_mode(true);
}

void DeviceManagementScreen::keep_device_btn_cb(lv_event_t *)
{
    if (g_screen) g_screen->set_delete_confirm_mode(false);
}

// hub side only, Zigbee still lingers
void DeviceManagementScreen::confirm_delete_btn_cb(lv_event_t *)
{
    if (!g_screen) return;
    g_screen->model->remove_device(g_screen->popup_device_id);
    g_screen->close_edit_popup();
}

void DeviceManagementScreen::build_row(uint64_t id, const UiDevice &dev)
{
    lv_obj_t *row = lv_obj_create(device_list);
    lv_obj_set_size(row, LV_PCT(100), 54);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *text_col = lv_obj_create(row);
    lv_obj_set_size(text_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(text_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_col, 0, 0);
    lv_obj_set_style_pad_all(text_col, 0, 0);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_CLICKABLE); // let taps fall through to the row
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *name = lv_label_create(text_col);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *watt = lv_label_create(text_col);
    lv_obj_set_style_text_color(watt, lv_color_hex(0x999999), 0);

    lv_obj_t *tag = create_priority_tag(row, dev.priority);

    DeviceRow row_data;
    row_data.container = row;
    row_data.name_label = name;
    row_data.watt_label = watt;
    row_data.tag = tag;
    row_data.id_ctx = new uint64_t(id); // freed in on_device_removed
    lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, row_data.id_ctx);

    auto it = rows.emplace(id, row_data).first;
    apply_row(it->second, dev);
    update_empty_state();
}

void DeviceManagementScreen::apply_row(DeviceRow &row, const UiDevice &dev)
{
    lv_label_set_text(row.name_label, dev.name.c_str());

    char buf[24];
    if (dev.supports_metering) snprintf(buf, sizeof(buf), "%.0f W", dev.power);
    else snprintf(buf, sizeof(buf), "no metering");
    lv_label_set_text(row.watt_label, buf);

    set_priority_tag(row.tag, dev.priority);
    lv_obj_set_style_opa(row.container, dev.online ? LV_OPA_COVER : LV_OPA_50, 0);
}

void DeviceManagementScreen::refresh_connected_count()
{
    if (!subtitle_label) return;
    char buf[24];
    snprintf(buf, sizeof(buf), "%d Connected", (int)model->devices().size());
    lv_label_set_text(subtitle_label, buf);
}

void DeviceManagementScreen::update_empty_state()
{
    if (!empty_label) return;
    if (rows.empty()) lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
}

void DeviceManagementScreen::on_device_added(const UiDevice &dev)
{
    build_row(dev.id, dev);
    refresh_connected_count();
}

void DeviceManagementScreen::on_device_updated(const UiDevice &dev)
{
    auto it = rows.find(dev.id);
    if (it == rows.end()) build_row(dev.id, dev);
    else apply_row(it->second, dev);
    // if the popup is open for this device the user is mid-edit -> leave it alone, save/cancel will reconcile it
}

void DeviceManagementScreen::on_device_removed(uint64_t id)
{
    if (popup_overlay && popup_device_id == id) close_edit_popup(); // if device left mid-edit

    auto it = rows.find(id);
    if (it == rows.end()) return;
    delete it->second.id_ctx;
    lv_obj_delete(it->second.container);
    rows.erase(it);
    update_empty_state();
    refresh_connected_count();
}

void DeviceManagementScreen::select_priority(int priority)
{
    popup_priority = priority;
    for (int i = 0; i < 3; ++i) {
        if (!popup_priority_btns[i]) continue;
        uint32_t color = (i == priority) ? priority_color(i) : 0x333333;
        lv_obj_set_style_bg_color(popup_priority_btns[i], lv_color_hex(color), 0);
    }
}

void DeviceManagementScreen::set_delete_confirm_mode(bool confirming)
{
    auto show = [](lv_obj_t *obj, bool visible) {
        if (!obj) return;
        if (visible) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    };
    show(popup_cancel_btn, !confirming);
    show(popup_save_btn, !confirming);
    show(popup_keep_btn, confirming);
    show(popup_confirm_delete_btn, confirming);
}

void DeviceManagementScreen::open_edit_popup(uint64_t id)
{
    const UiDevice *dev = model->find(id);
    if (!dev) return;
    if (popup_overlay) close_edit_popup();

    popup_device_id = id;

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
    lv_label_set_text(title, dev->pending ? "New device" : "Edit device");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *name_row = lv_obj_create(card);
    lv_obj_set_size(name_row, 416, 36);
    lv_obj_set_style_bg_opa(name_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(name_row, 0, 0);
    lv_obj_set_style_pad_all(name_row, 0, 0);
    lv_obj_clear_flag(name_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(name_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(name_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    popup_name_ta = lv_textarea_create(name_row);
    lv_obj_set_size(popup_name_ta, 372, 36);
    lv_textarea_set_one_line(popup_name_ta, true);
    lv_textarea_set_max_length(popup_name_ta, sizeof(UiDeviceRecord::name) - 1);
    lv_textarea_set_placeholder_text(popup_name_ta, "Device name");
    lv_textarea_set_text(popup_name_ta, dev->name.c_str());

    // tap swaps the button row below into keep/confirm - see set_delete_confirm_mode.
    lv_obj_t *delete_btn = create_icon_button(name_row, LV_SYMBOL_TRASH, 0x7A1F1F);
    lv_obj_add_event_cb(delete_btn, delete_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *prio_row = lv_obj_create(card);
    lv_obj_set_size(prio_row, 416, 40);
    lv_obj_set_style_bg_opa(prio_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(prio_row, 0, 0);
    lv_obj_set_style_pad_all(prio_row, 0, 0);
    lv_obj_clear_flag(prio_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(prio_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(prio_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    static constexpr int PRIORITY_DISPLAY_ORDER[3] = {1, 2, 0};
    for (int priority : PRIORITY_DISPLAY_ORDER) {
        lv_obj_t *btn = lv_button_create(prio_row);
        lv_obj_set_size(btn, 132, 36);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, priority_label(priority));
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, priority_btn_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(priority)));
        popup_priority_btns[priority] = btn;
    }
    select_priority(dev->priority);

    lv_obj_t *btn_row = lv_obj_create(card);
    lv_obj_set_size(btn_row, 416, 40);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    popup_cancel_btn = lv_button_create(btn_row);
    lv_obj_set_size(popup_cancel_btn, 200, 40);
    lv_obj_set_style_bg_color(popup_cancel_btn, lv_color_hex(0x333333), 0);
    lv_obj_t *cancel_lbl = lv_label_create(popup_cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(popup_cancel_btn, cancel_btn_cb, LV_EVENT_CLICKED, NULL);

    popup_save_btn = lv_button_create(btn_row);
    lv_obj_set_size(popup_save_btn, 200, 40);
    lv_obj_set_style_bg_color(popup_save_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_t *save_lbl = lv_label_create(popup_save_btn);
    lv_label_set_text(save_lbl, "Save");
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(popup_save_btn, save_btn_cb, LV_EVENT_CLICKED, NULL);

    // confirm set: hidden until the trash icon is tapped once (see set_delete_confirm_mode)
    popup_keep_btn = lv_button_create(btn_row);
    lv_obj_set_size(popup_keep_btn, 200, 40);
    lv_obj_set_style_bg_color(popup_keep_btn, lv_color_hex(0x333333), 0);
    lv_obj_t *keep_lbl = lv_label_create(popup_keep_btn);
    lv_label_set_text(keep_lbl, "Keep device");
    lv_obj_center(keep_lbl);
    lv_obj_add_event_cb(popup_keep_btn, keep_device_btn_cb, LV_EVENT_CLICKED, NULL);

    popup_confirm_delete_btn = lv_button_create(btn_row);
    lv_obj_set_size(popup_confirm_delete_btn, 200, 40);
    lv_obj_set_style_bg_color(popup_confirm_delete_btn, lv_color_hex(0xE53935), 0);
    lv_obj_t *confirm_lbl = lv_label_create(popup_confirm_delete_btn);
    lv_label_set_text(confirm_lbl, "Confirm delete");
    lv_obj_center(confirm_lbl);
    lv_obj_add_event_cb(popup_confirm_delete_btn, confirm_delete_btn_cb, LV_EVENT_CLICKED, NULL);

    set_delete_confirm_mode(false);

    // keyboard is visible while the popup is open
    lv_obj_t *keyboard = lv_keyboard_create(popup_overlay);
    lv_obj_set_size(keyboard, LV_PCT(100), 118);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(keyboard, popup_name_ta);
}

void DeviceManagementScreen::save_edit_popup()
{
    if (!popup_overlay || !popup_name_ta) return;
    const char *text = lv_textarea_get_text(popup_name_ta);
    if (text == nullptr || text[0] == '\0') return; // ignores empty name and keep the popup open

    model->name_device(popup_device_id, text, popup_priority);
    close_edit_popup();
}

void DeviceManagementScreen::close_edit_popup()
{
    if (popup_overlay) {
        lv_obj_delete(popup_overlay);
        popup_overlay = nullptr;
    }
    popup_name_ta = nullptr;
    for (auto &btn : popup_priority_btns) btn = nullptr;
    popup_cancel_btn = nullptr;
    popup_save_btn = nullptr;
    popup_keep_btn = nullptr;
    popup_confirm_delete_btn = nullptr;
}

lv_obj_t *DeviceManagementScreen::build(UiModel &m)
{
    model = &m;
    g_screen = this;

    lv_obj_t *header;
    lv_obj_t *scr = create_screen_with_header("Device Management", "0 Connected", &header, &subtitle_label);

    lv_obj_t *back_btn = create_icon_button(header, LV_SYMBOL_LEFT, 0x4CAF50);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *add_btn = lv_button_create(header);
    lv_obj_set_size(add_btn, 116, 34);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(0x333333), 0);
    lv_obj_align_to(add_btn, back_btn, LV_ALIGN_OUT_LEFT_MID, -8, 0);
    add_device_label = lv_label_create(add_btn);
    lv_label_set_text(add_device_label, "+ Add device");
    lv_obj_set_style_text_color(add_device_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(add_device_label);
    lv_obj_add_event_cb(add_btn, add_device_btn_cb, LV_EVENT_CLICKED, NULL);

    device_list = lv_obj_create(scr);
    lv_obj_set_size(device_list, 460, 224);
    lv_obj_align(device_list, LV_ALIGN_TOP_MID, 0, 74);
    lv_obj_set_style_bg_color(device_list, lv_color_hex(0x262626), 0);
    lv_obj_set_style_radius(device_list, 12, 0);
    lv_obj_set_style_border_width(device_list, 0, 0);
    lv_obj_set_style_pad_all(device_list, 8, 0);
    lv_obj_set_flex_flow(device_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(device_list, LV_DIR_VER);

    empty_label = lv_label_create(device_list);
    lv_label_set_text(empty_label, "No devices yet. Tap + Add device to pair a smart plug.");
    lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);

    refresh_connected_count();
    update_empty_state();

    model->add_listener(this);
    return scr;
}

} // namespace

lv_obj_t *create_device_management_screen(UiModel &model)
{
    static DeviceManagementScreen screen;
    return screen.build(model);
}

void device_management_open_edit_popup(uint64_t id)
{
    if (g_screen) g_screen->open_edit_popup(id);
}
