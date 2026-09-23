#include "home_screen.h"
#include "screen_common.h"
#include "screen_manager.h"

#include <cstdio>
#include <map>

namespace {

constexpr uint32_t CHIP_BG = 0x262626;
constexpr uint32_t CARD_BG = 0x262626;
constexpr uint32_t PRICE_COLOR = 0x4CAF50;
constexpr uint32_t USAGE_COLOR = 0xFB8C00;
constexpr uint32_t SWITCH_ON_COLOR = 0x4CAF50;

// a live device list below 

// home screen implements UiModelListener and is kept as a function
// local static in create_home_screen() -> it (and every lv_obj_t it owns) lives for as long as
// the app runs, same as the other screens (screen_manager never destroys a screen, just hides it) this is better imo

// threshold chip shows both cutoffs now (settings will have both sliders)

class HomeScreen : public UiModelListener {
public:
    lv_obj_t *build(UiModel &model);

private:
    struct SwitchCtx {
        HomeScreen *screen;
        uint64_t device_id;
    };

    struct DeviceRow {
        lv_obj_t *container{};
        lv_obj_t *name_label{};
        lv_obj_t *priority_label_obj{};
        lv_obj_t *sw{};
        SwitchCtx *ctx{};
    };

    void on_device_added(const UiDevice &dev) override;
    void on_device_updated(const UiDevice &dev) override;
    void on_device_removed(uint64_t id) override;
    void on_price_changed(float price) override;
    void on_thresholds_changed(float low, float med) override;

    lv_obj_t *build_chip(lv_obj_t *parent, int x, int width, const char *label, uint32_t value_color, lv_obj_t **out_value);
    void build_threshold_chip(lv_obj_t *parent, int x, int width);
    void build_row(uint64_t id, const UiDevice &dev);
    void apply_row(DeviceRow &row, const UiDevice &dev);
    void refresh_usage();
    void refresh_price_text();
    void refresh_threshold_text();
    void update_empty_state();

    static void switch_event_cb(lv_event_t *e);
    static void add_btn_cb(lv_event_t *e);
    static void settings_btn_cb(lv_event_t *e);

    UiModel *model{};
    lv_obj_t *device_list{};
    lv_obj_t *empty_label{};
    lv_obj_t *price_value{};
    lv_obj_t *threshold_low_value{};
    lv_obj_t *threshold_med_value{};
    lv_obj_t *usage_value{};

    std::map<uint64_t, DeviceRow> rows;
};

void HomeScreen::add_btn_cb(lv_event_t *)
{
    screen_manager_show(ScreenId::DEVICE_MANAGEMENT);
}

void HomeScreen::settings_btn_cb(lv_event_t *)
{
    screen_manager_show(ScreenId::SETTINGS);
}

void HomeScreen::switch_event_cb(lv_event_t *e)
{
    auto *ctx = static_cast<SwitchCtx *>(lv_event_get_user_data(e));
    auto *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ctx->screen->model->set_plug(ctx->device_id, on);
}

lv_obj_t *HomeScreen::build_chip(lv_obj_t *parent, int x, int width, const char *label, uint32_t value_color, lv_obj_t **out_value)
{
    lv_obj_t *chip = lv_obj_create(parent);
    lv_obj_set_size(chip, width, 54);
    lv_obj_align(chip, LV_ALIGN_LEFT_MID, x, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(CHIP_BG), 0);
    lv_obj_set_style_radius(chip, 10, 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_pad_all(chip, 6, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *val = lv_label_create(chip);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, lv_color_hex(value_color), 0);
    lv_obj_align(val, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lbl = lv_label_create(chip);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x999999), 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    if (out_value) *out_value = val;
    return chip;
}

void HomeScreen::build_threshold_chip(lv_obj_t *parent, int x, int width)
{
    lv_obj_t *chip = lv_obj_create(parent);
    lv_obj_set_size(chip, width, 54);
    lv_obj_align(chip, LV_ALIGN_LEFT_MID, x, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(CHIP_BG), 0);
    lv_obj_set_style_radius(chip, 10, 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_pad_all(chip, 6, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *value_row = lv_obj_create(chip);
    lv_obj_set_size(value_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(value_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(value_row, 0, 0);
    lv_obj_set_style_pad_all(value_row, 0, 0);
    lv_obj_clear_flag(value_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(value_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(value_row, 3, 0);
    lv_obj_align(value_row, LV_ALIGN_TOP_LEFT, 0, 0);

    threshold_low_value = lv_label_create(value_row);
    lv_label_set_text(threshold_low_value, "--");
    lv_obj_set_style_text_color(threshold_low_value, lv_color_hex(priority_color(1)), 0);

    lv_obj_t *sep = lv_label_create(value_row);
    lv_label_set_text(sep, "/");
    lv_obj_set_style_text_color(sep, lv_color_hex(0x666666), 0);

    threshold_med_value = lv_label_create(value_row);
    lv_label_set_text(threshold_med_value, "--");
    lv_obj_set_style_text_color(threshold_med_value, lv_color_hex(priority_color(2)), 0);

    lv_obj_t *lbl = lv_label_create(chip);
    lv_label_set_text(lbl, "Threshold");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x999999), 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void HomeScreen::build_row(uint64_t id, const UiDevice &dev)
{
    lv_obj_t *row = lv_obj_create(device_list);
    lv_obj_set_size(row, LV_PCT(100), 50);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *text_col = lv_obj_create(row);
    lv_obj_set_size(text_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(text_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_col, 0, 0);
    lv_obj_set_style_pad_all(text_col, 0, 0);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *name = lv_label_create(text_col);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *prio = lv_label_create(text_col);
    lv_obj_set_style_text_color(prio, lv_color_hex(0x999999), 0);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_style_bg_color(sw, lv_color_hex(SWITCH_ON_COLOR), LV_PART_INDICATOR | LV_STATE_CHECKED);

    DeviceRow row_data;
    row_data.container = row;
    row_data.name_label = name;
    row_data.priority_label_obj = prio;
    row_data.sw = sw;
    row_data.ctx = new SwitchCtx{this, id}; // freed in on_device_removed; rows live until the device leaves
    lv_obj_add_event_cb(sw, switch_event_cb, LV_EVENT_VALUE_CHANGED, row_data.ctx);

    auto it = rows.emplace(id, row_data).first;
    apply_row(it->second, dev);
    update_empty_state();
}

void HomeScreen::apply_row(DeviceRow &row, const UiDevice &dev)
{
    lv_label_set_text(row.name_label, dev.name.c_str());
    lv_label_set_text(row.priority_label_obj, dev.pending ? "Tap to set up" : priority_label(dev.priority));

    if (dev.on) lv_obj_add_state(row.sw, LV_STATE_CHECKED);
    else lv_obj_clear_state(row.sw, LV_STATE_CHECKED);

    // offline devices should just dim for now!
    lv_obj_set_style_opa(row.container, dev.online ? LV_OPA_COVER : LV_OPA_50, 0);
}

void HomeScreen::refresh_usage()
{
    float total_w = 0.0f;
    for (const auto &entry : model->devices()) total_w += entry.second.power;

    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f kW", total_w / 1000.0f);
    lv_label_set_text(usage_value, buf);
}

void HomeScreen::refresh_price_text()
{
    char buf[24];
    if (model->has_price()) snprintf(buf, sizeof(buf), "%.2f c/kWh", model->price());
    else snprintf(buf, sizeof(buf), "-- c/kWh");
    lv_label_set_text(price_value, buf);
}

void HomeScreen::refresh_threshold_text()
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%.1f", model->threshold_low());
    lv_label_set_text(threshold_low_value, buf);

    snprintf(buf, sizeof(buf), "%.1f", model->threshold_medium());
    lv_label_set_text(threshold_med_value, buf);
}

void HomeScreen::update_empty_state()
{
    if (!empty_label) return;
    if (rows.empty()) lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
}

void HomeScreen::on_device_added(const UiDevice &dev)
{
    build_row(dev.id, dev);
}

void HomeScreen::on_device_updated(const UiDevice &dev)
{
    auto it = rows.find(dev.id);
    if (it == rows.end()) {
        build_row(dev.id, dev);
    } else {
        apply_row(it->second, dev);
    }
    refresh_usage();
}

void HomeScreen::on_device_removed(uint64_t id)
{
    auto it = rows.find(id);
    if (it == rows.end()) return;
    delete it->second.ctx;
    lv_obj_delete(it->second.container);
    rows.erase(it);
    update_empty_state();
    refresh_usage();
}

void HomeScreen::on_price_changed(float)
{
    refresh_price_text();
}

void HomeScreen::on_thresholds_changed(float, float)
{
    refresh_threshold_text();
}

lv_obj_t *HomeScreen::build(UiModel &m)
{
    model = &m;

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1A1A1A), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *top_bar = lv_obj_create(screen);
    lv_obj_set_size(top_bar, 460, 54);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    build_chip(top_bar, 0, 104, "Price", PRICE_COLOR, &price_value);
    build_threshold_chip(top_bar, 112, 130);
    build_chip(top_bar, 250, 104, "Usage", USAGE_COLOR, &usage_value);

    lv_obj_t *add_btn = create_icon_button(top_bar, LV_SYMBOL_PLUS, 0x333333);
    lv_obj_align(add_btn, LV_ALIGN_RIGHT_MID, -44, 0);
    lv_obj_add_event_cb(add_btn, add_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *settings_btn = create_icon_button(top_bar, LV_SYMBOL_SETTINGS, 0x333333);
    lv_obj_align(settings_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(settings_btn, settings_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *section_label = lv_label_create(screen);
    lv_label_set_text(section_label, "Devices");
    lv_obj_set_style_text_color(section_label, lv_color_hex(0x999999), 0);
    lv_obj_align(section_label, LV_ALIGN_TOP_LEFT, 14, 74);

    device_list = lv_obj_create(screen);
    lv_obj_set_size(device_list, 460, 210);
    lv_obj_align(device_list, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_set_style_bg_color(device_list, lv_color_hex(CARD_BG), 0);
    lv_obj_set_style_radius(device_list, 12, 0);
    lv_obj_set_style_border_width(device_list, 0, 0);
    lv_obj_set_style_pad_all(device_list, 8, 0);
    lv_obj_set_flex_flow(device_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(device_list, LV_DIR_VER);

    empty_label = lv_label_create(device_list);
    lv_label_set_text(empty_label, "No devices yet. Tap + to add one.");
    lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);

    refresh_price_text();
    refresh_threshold_text();
    refresh_usage();
    update_empty_state();

    model->add_listener(this);
    return screen;
}

} // namespace

lv_obj_t *create_home_screen(UiModel &model)
{
    static HomeScreen home_screen;
    return home_screen.build(model);
}
