#ifndef DEVICEINFOSTORAGE_H
#define DEVICEINFOSTORAGE_H

static const char *TAG = "DEV_STORAGE"; 

template<typename T>
class DeviceInfoStorage {
public:
    DeviceInfoStorage(const std::string &ns_name, const std::string &key_name) : storage(ns_name), keyName(key_name) {
        // automatically read vector from memory at boot
        esp_err_t err = storage.read_vector(keyName, deviceCache); 

        if (err == ESP_OK) ESP_LOGI(TAG, "Successfully read info from nvs namespace: %s with key: %s", ns_name, keyName);
        else if (err == ESP_ERR_NVS_NOT_FOUND) ESP_LOGW(TAG, "Namespace: %s and key: %s empty.", ns_name, keyName);
        else ESP_LOGE(TAG, "error: (%s) while reading namespace: %s with key: %s", esp_err_to_name(err), ns_name, keyName); 

    }
    ~DeviceInfoStorage() = default; 

    esp_err_t save_device(T &device_struct) {
        auto it = std::ranges::find_if(deviceCache, 
            [&] (const auto& cache) { 
                return cache.dev_id == device_struct.dev_id;
            }
        );
        if (it != deviceCache.end()) {
            ESP_LOGI(TAG, "Updating existing dev.");
            *it = device_struct;
        } else {
            ESP_LOGI(TAG, "adding new device");
            deviceCahce.push_back(device_struct);
        }
        return storage.write_vector(keyName, deviceCache);
    }

    const std::vector<T>& get_all_devices() const{
        return deviceCache; 
    }

    esp_err_t delete_device_from_memroy(uint64_t dev_id) {
        auto it = std::ranges::find_if(deviceCache, 
            [&] (const auto& cache) { 
                return cache.dev_id == dev_id;
            }
        );
        if (it != deviceCache.end()) {
            deviceCache.erase(it);
            return storage.write_vector(keyName, deviceCache); 
        }
        ESP_LOGE(TAG, "trying to delete a device which does not exist on device cache");
        return ESP_ERR_NVS_NOT_FOUND // or something else here
    }

private:
    NvsStorage storage; 
    std::string keyName; 
    std::vector<T> deviceCache; 
};

#endif //DEVICEINFOSTORAGE_H