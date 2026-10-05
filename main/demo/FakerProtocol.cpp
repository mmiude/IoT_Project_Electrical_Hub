#include "FakerProtocol.h"

FakerProtocol::FakerProtocol(QueueHandle_t controller_q, EventGroupHandle_t e_bits, std::shared_ptr<DeviceInfoStorage<f_dev>> storage) : controller_queue(controller_q), event_group(e_bits), dev_storage(storage) {
    //init_demo_devices(); 

    // was 10, maybe dropping something?
    f_event_queue = xQueueCreate(40, sizeof(f_data));
    xTaskCreate(FakerProtocol::runner, "FAKE_PROTOCOL", 4096, this, tskIDLE_PRIORITY + 1, &handle);
}

void FakerProtocol::init_demo_devices() {
    // fake fridge
    f_devices[1001] = f_dev{
        .dev_id = 1001,
        .active_power = 120.0,
        .idle_power = 4.0,
        .is_on = true,
        .total_energy_consumption = 0.5,
        .last_energy_update = 0,
    };
    // fake floor heating
    f_devices[1002] = f_dev{
        .dev_id = 1002,
        .active_power = 2200.0,
        .idle_power = 0.0,
        .is_on = true, 
        .total_energy_consumption = 0.2,
        .last_energy_update = 0,
    };
    // fake air conditioner
    f_devices[1003] = f_dev{
        .dev_id = 1003, 
        .active_power = 900.0,
        .idle_power = 12.0,
        .is_on = false,
        .total_energy_consumption = 0.3,
        .last_energy_update = 0,
    };
}

void FakerProtocol::runner(void *params){
    auto instance = static_cast<FakerProtocol *> (params);
    xEventGroupWaitBits(instance->event_group, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY); // wait that zigbee stack is initialized 
    instance->run();
}

void FakerProtocol::run() {
    
    dev_storage->get_all_devices(f_devices); 
    const std::vector<uint64_t> expected_devs = {1001, 1002, 1003};

    if (f_devices.size() < 3) {  // check if some of the demo devices has been erased -> send dev join data type to add them again
        for (auto e : expected_devs) {
            if (f_devices.find(e) == f_devices.end()) {
                controller_data data = {.device_id = e, .type = DATA_TYPE_DEVICE_JOIN, .data = {.value_int = FAKER}};
                xQueueSendToBack(controller_queue, &data, 0);
                vTaskDelay(10);
                // this v - rene
                controller_data metering_data = {.device_id = e, .type = DATA_TYPE_SUPPORTS_METERING, .data = {.flag = true}};
                xQueueSendToBack(controller_queue, &metering_data, 0);
                vTaskDelay(10);
            }
        }
    }

    init_demo_devices(); // we init all devices any way -> overwriting does not matter in this case...
    for (auto &[key, dev] : f_devices) dev_storage->save_device(key, dev); // save all for nex time... for demo this is good enough... 

    f_data event{};
    controller_data ctrl_data{};

    while (true) {
        if (xQueueReceive(f_event_queue, &event, portMAX_DELAY)) {
            auto dev = find_dev(event.dev_id);

            switch (event.type) {
                case ENERGY_CONSUMPTION:
                    if (dev) {
                        vTaskDelay(50); // small delay to make it more realistic. 
                        ctrl_data = {.device_id = dev->dev_id, .type = DATA_TYPE_ENERGY, .data = {.value = dev->total_energy_consumption}};
                        xQueueSendToBack(controller_queue, &ctrl_data, 0); 
                    }
                    break;
                case VOLTAGE:
                    if (dev) {
                        vTaskDelay(10); // small delay to make it more realistic. 
                        ctrl_data = {.device_id = dev->dev_id, .type = DATA_TYPE_VOLTAGE, .data = {.value = event.data.f_value}};
                        xQueueSendToBack(controller_queue, &ctrl_data, 0); 
                    }
                    break;
                case CURRENT:
                     if (dev) {
                        vTaskDelay(10); // small delay to make it more realistic. 
                        ctrl_data = {.device_id = dev->dev_id, .type = DATA_TYPE_CURRENT, .data = {.value = event.data.f_value}};
                        xQueueSendToBack(controller_queue, &ctrl_data, 0); 
                    }
                    break;
                case POWER:
                     if (dev) {
                        vTaskDelay(10); // small delay to make it more realistic. 
                        ctrl_data = {.device_id = dev->dev_id, .type = DATA_TYPE_POWER, .data = {.value = event.data.f_value}};
                        xQueueSendToBack(controller_queue, &ctrl_data, 0); 
                    }
                    break;
                case STATE: 
                     if (dev) {
                        vTaskDelay(10); // small delay to make it more realistic. 
                        ctrl_data = {.device_id = dev->dev_id, .type = DATA_TYPE_SET_ON, .data = {.flag = event.data.flag}};
                        xQueueSendToBack(controller_queue, &ctrl_data, 0); 
                    }
                    break;
                default: 
                    ESP_LOGE("FAKER:", "received unknown event.");
                    break;
            }
        }
    }
}

void FakerProtocol::request_energy_consumption_values(uint64_t device_id){
    f_data data{};
    auto dev = find_dev(device_id);
    if (dev) {
        data = {.type = ENERGY_CONSUMPTION, .dev_id = device_id, .data = {.f_value = get_energy_consumption(*dev)} };
        xQueueSendToBack(f_event_queue, &data, 0);
    }
}

void FakerProtocol::request_electrical_values(uint64_t device_id){
    f_data data_v{.type = VOLTAGE, .dev_id = device_id};
    f_data data_p{.type = POWER, .dev_id = device_id};
    f_data data_c{.type = CURRENT, .dev_id = device_id};

    auto dev = find_dev(device_id);
    if (dev) {
        data_v.data.f_value = get_voltage(*dev);
        data_p.data.f_value = get_power(*dev);
        data_c.data.f_value = get_current(*dev); 
    }
    xQueueSendToBack(f_event_queue, &data_v, 0);
    xQueueSendToBack(f_event_queue, &data_p, 0);
    xQueueSendToBack(f_event_queue, &data_c, 0);
}

void FakerProtocol::request_on_off_state(uint64_t device_id){
    f_data data{};
    auto dev = find_dev(device_id);

    if (dev) {
        data = {.type = STATE, .dev_id = device_id, .data = {.flag = dev->is_on}};
        xQueueSendToBack(f_event_queue, &data, 0);
    }
}

void FakerProtocol::toggle_plug(uint64_t device_id){
    f_data data{};
    auto dev = find_dev(device_id);

    if (dev) {
        dev->is_on = !dev->is_on; 
        data = {.type = STATE, .dev_id = device_id, .data = {.flag = dev->is_on}};
        xQueueSendToBack(f_event_queue, &data, 0);
    }
}

void FakerProtocol::set_plug_on(uint64_t device_id){
    f_data data{};
    auto dev = find_dev(device_id);

    if (dev) {
        dev->is_on = true; 
        data = {.type = STATE, .dev_id = device_id, .data = {.flag = dev->is_on}};
        xQueueSendToBack(f_event_queue, &data, 0);
    }
}

void FakerProtocol::set_plug_off(uint64_t device_id){
    f_data data{};
    auto dev = find_dev(device_id);

    if (dev) {
        dev->is_on = false; 
        data = {.type = STATE, .dev_id = device_id, .data = {.flag = dev->is_on}};
        xQueueSendToBack(f_event_queue, &data, 0);
    }
}   

void FakerProtocol::open_network() {
    ESP_LOGI("FAKER:", "opening network..."); 
}

void FakerProtocol::delete_device(uint64_t device_id){
    auto dev = find_dev(device_id);
    if (dev) {
        f_devices.erase(device_id);
        dev_storage->delete_device_from_memory(device_id);
        ESP_LOGI("FAKER", "deleting device from f_dev map.");
    } else ESP_LOGE("FAKER", "trying to delete unknown f_dev.");
    ESP_LOGI("FAKER:", "map size: %d", f_devices.size());
}

float FakerProtocol::get_voltage(const f_dev &dev){
    if (!dev.is_on) return 0.0; 
    return 230.0 * get_noise(0.01);
}

float FakerProtocol::get_power(const f_dev &dev){
    if (!dev.is_on) return 0.0; 

    if (dev.dev_id == 1001) { // simulate fridge compressor 
        TickType_t now = xTaskGetTickCount();
        uint32_t total_seconds = now / configTICK_RATE_HZ;

        // Cycles every 20 minutes (1200s). Compressor runs for first 5 minutes (300s)
        bool compressor_running = (total_seconds % 1200) < 300;

        float base = compressor_running ? dev.active_power : dev.idle_power;
        return base * get_noise(0.02f);
    }

    return dev.active_power * get_noise(0.02);
}

float FakerProtocol::get_current(const f_dev &dev){
    if (!dev.is_on) return 0.0; 

    float voltage = get_voltage(dev);
    float power = get_power(dev);

    return (voltage > 0.0) ? (power / voltage) : 0.0; 
}

float FakerProtocol::get_energy_consumption(f_dev &dev){
    TickType_t now = xTaskGetTickCount();
    
    TickType_t elapsed_ticks = now - dev.last_energy_update;
    
    if (dev.is_on && elapsed_ticks > 0) {
        // Convert ticks to seconds, then to hours
        double elapsed_seconds = static_cast<double>(elapsed_ticks) / configTICK_RATE_HZ;
        double elapsed_hours = elapsed_seconds / 3600.0;
        
        float current_power = get_power(dev);
        
        // Accumulate energy: kWh = (Watts * hours) / 1000
        dev.total_energy_consumption += (current_power * elapsed_hours) / 1000.0;
    }
    
    dev.last_energy_update = now;
    return dev.total_energy_consumption;
}

float FakerProtocol::get_noise(float per) {
        float random_noise = static_cast<float>(rand() / static_cast<float>(RAND_MAX));
        return 1.0 + ((random_noise * 2.0 - 1.0) * per);
}
 
f_dev* FakerProtocol::find_dev(uint64_t id) {
    auto it = f_devices.find(id);
    return (it != f_devices.end()) ? &it->second : nullptr; 
}

