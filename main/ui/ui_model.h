#ifndef UI_MODEL_H
#define UI_MODEL_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "HubControllerEnums.h"
#include "DeviceInfoStorage.h"

// what the ui keeps on nvs per device ID 
// controller owns priority/thresholds and ui only owns the name

struct UiDeviceRecord {
    char name[32]{};
};

struct UiDevice {
    uint64_t id{};
    std::string name;
    int priority{0};
    bool on{false};
    bool online{false};
    bool supports_metering{false}; // checking if the plug supports power reporting
    float power{0.0f};
    float energy{0.0f};

    bool pending{true};     // joined but the user has not given it a name + priority yet!

    // for sync 
    bool confirmed{false};  // seen from the controller since boot (used to drop stale devices after sync)
};

class UiModelListener {
public:
    virtual ~UiModelListener() = default;
    virtual void on_device_added(const UiDevice &dev) {}
    virtual void on_device_updated(const UiDevice &dev) {}
    virtual void on_device_removed(uint64_t id) {}
    virtual void on_price_changed(float price) {}
    virtual void on_thresholds_changed(float low, float med) {}
};

// controller messages (ui queue) -> ui: device map, price, thresholds
// persists device names and sends user actions to the controller -> controller queue

class UiModel {
public:
    UiModel(QueueHandle_t controller_queue, std::shared_ptr<DeviceInfoStorage<UiDeviceRecord>> storage);

    void add_listener(UiModelListener *listener);

    // restores named devices from nvs and call before screens are created
    void load();
    // asks the controller to replay its state, call once the ui is ready to receive!
    void request_sync();

    // applies one message coming from the controller
    void handle_message(const controller_data &msg);

    // user actions -> controller
    void open_network();
    void set_plug(uint64_t id, bool on);
    void toggle_plug(uint64_t id);
    void set_threshold_low(float value);
    void set_threshold_medium(float value);

    // gives a pending device its name + priority (saves name to nvs, sends priority to controller)
    void name_device(uint64_t id, const std::string &name, int priority);
    void set_priority(uint64_t id, int priority);

    // NOTE; you need to press reset pin on plugs after removing a device! doesn't remove the known device from zigbee network :(
    void remove_device(uint64_t id);

    const std::map<uint64_t, UiDevice> &devices() const { return device_map; }
    const UiDevice *find(uint64_t id) const;
    bool has_price() const { return price_known; }
    float price() const { return current_price; }
    float threshold_low() const { return low_threshold; }
    float threshold_medium() const { return med_threshold; }

private:
    QueueHandle_t controller_queue;
    std::shared_ptr<DeviceInfoStorage<UiDeviceRecord>> storage;
    std::vector<UiModelListener *> listeners;

    std::map<uint64_t, UiDevice> device_map;
    float current_price{0.0f};
    bool price_known{false};
    float low_threshold{0.0f};
    float med_threshold{0.0f};

    bool send(const controller_data &msg);
    void save_name(const UiDevice &dev);

    // for cleanup
    void prune_unconfirmed();

    void notify_added(const UiDevice &dev);
    void notify_updated(const UiDevice &dev);
    void notify_removed(uint64_t id);

    void notify_price();
    void notify_thresholds();
};

#endif // UI_MODEL_H
