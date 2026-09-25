#include "new_device_notifier.h"
#include "device_management_screen.h"

#include <deque>

// made it seperate since can happen on any screen, reuses the same device management pop up for yes
// doesn't remove the zigbee network connection pairing tho (need to press reset button on plugs) :(

namespace {

class NewDeviceNotifier : public UiModelListener {
public:
    void init(UiModel &m);

private:
    void on_device_added(const UiDevice &dev) override;
    void on_device_removed(uint64_t id) override;

    void show_next();
    void close_prompt();

    static void yes_btn_cb(lv_event_t *e);
    static void no_btn_cb(lv_event_t *e);

    UiModel *model{};
    lv_obj_t *overlay{};
    uint64_t active_id{0};

    // if multiple join and are waiting to be connected 
    std::deque<uint64_t> queue;
};

NewDeviceNotifier *g_notifier = nullptr;

void NewDeviceNotifier::yes_btn_cb(lv_event_t *)
{
    if (!g_notifier) return;
    uint64_t id = g_notifier->active_id;
    g_notifier->close_prompt();
    device_management_open_edit_popup(id);
    g_notifier->show_next();
}

void NewDeviceNotifier::no_btn_cb(lv_event_t *)
{
    if (!g_notifier) return;
    uint64_t id = g_notifier->active_id;
    g_notifier->model->remove_device(id);
    g_notifier->close_prompt();
    g_notifier->show_next();
}

void NewDeviceNotifier::on_device_added(const UiDevice &dev)
{
    queue.push_back(dev.id);
    if (!overlay) show_next();
}

void NewDeviceNotifier::on_device_removed(uint64_t id)
{
    if (active_id == id) {
        close_prompt();
        show_next();
        return;
    }
    for (auto it = queue.begin(); it != queue.end(); ++it) {
        if (*it == id) {
            queue.erase(it);
            break;
        }
    }
}

void NewDeviceNotifier::close_prompt()
{
    if (overlay) {
        lv_obj_delete(overlay);
        overlay = nullptr;
    }
    active_id = 0;
}

void NewDeviceNotifier::show_next()
{
    if (overlay || queue.empty()) return;

    active_id = queue.front();
    queue.pop_front();

    const UiDevice *dev = model->find(active_id);
    if (!dev) {
        show_next(); // gone already 
        return;
    }

    overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = lv_obj_create(overlay);
    lv_obj_set_size(card, 320, 168);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x262626), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "New device detected!");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *name_lbl = lv_label_create(card);
    lv_label_set_text(name_lbl, dev->name.c_str());
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(0x999999), 0);

    lv_obj_t *body = lv_label_create(card);
    lv_label_set_text(body, "Would you like to add it?");
    lv_obj_set_style_text_color(body, lv_color_hex(0xCCCCCC), 0);

    lv_obj_t *btn_row = lv_obj_create(card);
    lv_obj_set_size(btn_row, 288, 40);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *no_btn = lv_button_create(btn_row);
    lv_obj_set_size(no_btn, 138, 40);
    lv_obj_set_style_bg_color(no_btn, lv_color_hex(0x333333), 0);
    lv_obj_t *no_lbl = lv_label_create(no_btn);
    lv_label_set_text(no_lbl, "No");
    lv_obj_center(no_lbl);
    lv_obj_add_event_cb(no_btn, no_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *yes_btn = lv_button_create(btn_row);
    lv_obj_set_size(yes_btn, 138, 40);
    lv_obj_set_style_bg_color(yes_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_t *yes_lbl = lv_label_create(yes_btn);
    lv_label_set_text(yes_lbl, "Yes");
    lv_obj_center(yes_lbl);
    lv_obj_add_event_cb(yes_btn, yes_btn_cb, LV_EVENT_CLICKED, NULL);
}

void NewDeviceNotifier::init(UiModel &m)
{
    model = &m;
    g_notifier = this;
    model->add_listener(this);
}

} // namespace

void init_new_device_notifier(UiModel &model)
{
    static NewDeviceNotifier notifier;
    notifier.init(model);
}
