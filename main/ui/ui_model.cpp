#include "ui_model.h"

#include <cstdio>
#include <cstring>
#include "esp_log.h"

static const char *TAG = "UI_MODEL";

UiModel::UiModel(QueueHandle_t controller_queue, std::shared_ptr<DeviceInfoStorage<UiDeviceRecord>> storage)
    : controller_queue(controller_queue), storage(storage) {}

void UiModel::add_listener(UiModelListener *listener) {
    listeners.push_back(listener);
}

void UiModel::load() {
    std::map<uint64_t, UiDeviceRecord> saved;
    storage->get_all_devices(saved);

    for (const auto &[id, record] : saved) {
        UiDevice dev;
        dev.id = id;
        dev.name = std::string(record.name, strnlen(record.name, sizeof(record.name)));
        dev.pending = false;
        device_map[id] = dev; // state (online/on/priority) comes from the controller sync
    }
    ESP_LOGI(TAG, "restored %d named devices from nvs", (int)device_map.size());
}

void UiModel::request_sync() {
    controller_data msg = {.type = DATA_TYPE_UI_SYNC_REQUEST, .data = {}};
    send(msg);
}

void UiModel::handle_message(const controller_data &msg) {
    // device independent messages first
    switch (msg.type) {
        case DATA_TYPE_THRESHOLD_LOW:
            low_threshold = msg.data.value;
            notify_thresholds();
            return;
        case DATA_TYPE_THRESHOLD_MED:
            med_threshold = msg.data.value;
            notify_thresholds();
            return;
        case DATA_TYPE_ELEC_PRICE:
            current_price = msg.data.value;
            price_known = true;
            notify_price();
            return;
        case DATA_TYPE_UI_SYNC_DONE:
            prune_unconfirmed();
            return;
        default:
            break;
    }

    if (msg.type == DATA_TYPE_DEVICE_JOIN) {
        auto it = device_map.find(msg.device_id);
        if (it != device_map.end()) { // known from nvs (or replayed by sync)
            it->second.confirmed = true;
            notify_updated(it->second);
            return;
        }
        UiDevice dev;
        dev.id = msg.device_id;
        char default_name[32];
        snprintf(default_name, sizeof(default_name), "Device %04X", (unsigned)(msg.device_id & 0xFFFF));
        dev.name = default_name;
        dev.online = true;
        dev.confirmed = true;
        auto &stored = device_map[msg.device_id] = dev;
        ESP_LOGI(TAG, "new device 0x%016llx waiting for name/priority", msg.device_id);
        notify_added(stored);
        return;
    }

    if (msg.type == DATA_TYPE_DEVICE_LEFT) {
        auto it = device_map.find(msg.device_id);
        if (it == device_map.end()) return;
        if (!it->second.pending) storage->delete_device_from_memory(msg.device_id); // pending devices were never saved
        device_map.erase(it);
        notify_removed(msg.device_id);
        return;
    }

    auto it = device_map.find(msg.device_id);
    if (it == device_map.end()) {
        ESP_LOGW(TAG, "type %d for unknown device 0x%016llx", (int)msg.type, msg.device_id);
        return;
    }
    UiDevice &dev = it->second;

    switch (msg.type) {
        case DATA_TYPE_POWER:
            dev.power = msg.data.value;
            break;
        case DATA_TYPE_ENERGY:
            dev.energy = msg.data.value;
            break;
        case DATA_TYPE_SET_ON:
            dev.on = msg.data.flag;
            break;
        case DATA_TYPE_ONLINE_STATE:
            dev.online = msg.data.flag;
            break;
        case DATA_TYPE_SUPPORTS_METERING:
            dev.supports_metering = msg.data.flag;
            break;
        case DATA_TYPE_PRIORITY:
            dev.priority = msg.data.value_int;
            break;
        default:
            ESP_LOGI(TAG, "dev 0x%016llx: unhandled type %d", msg.device_id, (int)msg.type);
            return;
    }
    notify_updated(dev);
}

void UiModel::open_network() {
    controller_data msg = {.type = DATA_TYPE_COMMAND, .data = {.command = OPEN_NETWORK}};
    send(msg);
}

void UiModel::set_plug(uint64_t id, bool on) {
    controller_data msg = {.device_id = id, .type = DATA_TYPE_COMMAND, .data = {.command = on ? PLUG_ON : PLUG_OFF}};
    send(msg);
}

void UiModel::toggle_plug(uint64_t id) {
    controller_data msg = {.device_id = id, .type = DATA_TYPE_COMMAND, .data = {.command = TOGGLE_PLUG}};
    send(msg);
}

void UiModel::set_threshold_low(float value) {
    controller_data msg = {.type = DATA_TYPE_THRESHOLD_LOW, .data = {.value = value}};
    if (send(msg)) {
        low_threshold = value; // controller does not echo it back
        notify_thresholds();
    }
}

void UiModel::set_threshold_medium(float value) {
    controller_data msg = {.type = DATA_TYPE_THRESHOLD_MED, .data = {.value = value}};
    if (send(msg)) {
        med_threshold = value;
        notify_thresholds();
    }
}

void UiModel::name_device(uint64_t id, const std::string &name, int priority) {
    auto it = device_map.find(id);
    if (it == device_map.end()) return;
    UiDevice &dev = it->second;

    dev.name = name;
    dev.pending = false;
    save_name(dev);
    set_priority(id, priority); // also notifies listeners
}

void UiModel::set_priority(uint64_t id, int priority) {
    auto it = device_map.find(id);
    if (it == device_map.end()) return;

    controller_data msg = {.device_id = id, .type = DATA_TYPE_PRIORITY, .data = {.value_int = priority}};
    if (send(msg)) it->second.priority = priority;
    notify_updated(it->second);
}

void UiModel::remove_device(uint64_t id) {
    controller_data msg = {.device_id = id, .type = DATA_TYPE_COMMAND, .data = {.command = REMOVE_DEVICE}};
    send(msg);
}

void UiModel::set_wifi_credentials(const std::string &ssid, const std::string &password) {

    // just pondering ideas how to handle the wifi credential saving
    // ui has a working popup now that can be used to save ssid and pass :)
    (void)ssid;
    (void)password;
    // controller_data msg{};
    // msg.type = DATA_TYPE_WIFI_CREDENTIALS;
    // memset(&msg.data, 0, sizeof(msg.data)); // {} on the union alone only zeroes its first (float) member, not the full wifi struct - a max-length ssid/password would then read past the array with no null terminator otherwise
    // strncpy(msg.data.wifi.ssid, ssid.c_str(), sizeof(msg.data.wifi.ssid) - 1);
    // strncpy(msg.data.wifi.password, password.c_str(), sizeof(msg.data.wifi.password) - 1);
    // send(msg);
}

const UiDevice *UiModel::find(uint64_t id) const {
    auto it = device_map.find(id);
    return it == device_map.end() ? nullptr : &it->second;
}

bool UiModel::send(const controller_data &msg) {
    if (xQueueSendToBack(controller_queue, &msg, pdMS_TO_TICKS(20)) != pdPASS) {
        ESP_LOGE(TAG, "controller queue full, dropped message type %d", (int)msg.type);
        return false;
    }
    return true;
}

void UiModel::save_name(const UiDevice &dev) {
    UiDeviceRecord record;
    strncpy(record.name, dev.name.c_str(), sizeof(record.name) - 1);
    storage->save_device(dev.id, record);
}

// clean up! after a full sync anything we restored from nvs that the controller no longer knows has left the network
void UiModel::prune_unconfirmed() {
    for (auto it = device_map.begin(); it != device_map.end();) {
        if (it->second.confirmed) {
            ++it;
            continue;
        }
        ESP_LOGW(TAG, "dropping stale device 0x%016llx", it->first);
        uint64_t id = it->first;
        if (!it->second.pending) storage->delete_device_from_memory(id);
        it = device_map.erase(it);
        notify_removed(id);
    }
}

void UiModel::notify_added(const UiDevice &dev) {
    for (auto *l : listeners) l->on_device_added(dev);
}

void UiModel::notify_updated(const UiDevice &dev) {
    for (auto *l : listeners) l->on_device_updated(dev);
}

void UiModel::notify_removed(uint64_t id) {
    for (auto *l : listeners) l->on_device_removed(id);
}

void UiModel::notify_price() {
    for (auto *l : listeners) l->on_price_changed(current_price);
}

void UiModel::notify_thresholds() {
    for (auto *l : listeners) l->on_thresholds_changed(low_threshold, med_threshold);
}
