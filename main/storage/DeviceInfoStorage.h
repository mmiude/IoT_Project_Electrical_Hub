#ifndef DEVICEINFOSTORAGE_H
#define DEVICEINFOSTORAGE_H

template<typename T>
class DeviceInfoStorage {
public:
    DeviceInfoStorage(const std::string &ns_name, const std::string &key_name) : storage(ns_name), keyName(key_name) {
        // automatically read vector from memory at boot
        esp_err_t err = storage.read_vector(keyName, deviceCache); 

        if (err == ESP_OK) ESP_LOGI("DEV_STORAGE", "Successfully read info from nvs namespace: %s with key: %s", ns_name.c_str(), keyName.c_str());
        else if (err == ESP_ERR_NVS_NOT_FOUND) ESP_LOGW("DEV_STORAGE", "Namespace: %s and key: %s empty.", ns_name.c_str(), keyName.c_str());
        else ESP_LOGE("DEV_STORAGE", "error: (%s) while reading namespace: %s with key: %s", esp_err_to_name(err), ns_name.c_str(), keyName.c_str()); 

    }
    ~DeviceInfoStorage() = default; 

    esp_err_t save_device(const uint64_t dev_id, const T &device_struct) {
        auto it = std::ranges::find_if(deviceCache, 
            [&] (const auto& cache) { 
                return cache.first == dev_id;
            }
        );
        if (it != deviceCache.end()) {
            ESP_LOGI("DEV_STORAGE", "Updating existing dev.");
            it->second = device_struct;
        } else {
            ESP_LOGI("DEV_STORAGE", "adding new device");
            deviceCache.emplace_back(dev_id, device_struct);
        }
        return storage.write_vector(keyName, deviceCache);
    }

    esp_err_t get_all_devices(std::map<uint64_t, T> &devices) const{ // this should be called on during boot! 
        devices.clear();
        devices.insert(deviceCache.begin(), deviceCache.end());
        return ESP_OK; 
    }

    esp_err_t delete_device_from_memory(uint64_t dev_id) {
        auto it = std::ranges::find_if(deviceCache, 
            [&] (const auto& cache) { 
                return cache.first == dev_id;
            }
        );
        if (it != deviceCache.end()) {
            deviceCache.erase(it);
            return storage.write_vector(keyName, deviceCache); 
        }
        ESP_LOGE("DEV_STORAGE", "trying to delete a device which does not exist on device cache");
        return ESP_ERR_NVS_NOT_FOUND; // or something else here
    }

    esp_err_t eares_name_space() {
        return storage.erase_all(); 
    }

private:
    NvsStorage storage; 
    std::string keyName; 
    std::vector<std::pair<uint64_t, T>> deviceCache; 
};

#endif //DEVICEINFOSTORAGE_H