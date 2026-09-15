#ifndef NVSSTORAGE_H
#define NVSSTORAGE_H

#include "nvs_flash.h"
#include "esp_log.h"
#include <string>
#include <vector>

class NvsStorage {
public:
    NvsStorage(const std::string &nameSpace_n);
    ~NvsStorage();

    // restricted to trivially copyable types only!! - static_assert(std::is_trivially_copyable<T>::value???)
    template<typename T>
    esp_err_t read_blob(const std::string &key, T &data) { 
        if (!handle) return ESP_ERR_NVS_INVALID_HANDLE; 
        ESP_LOGW("NVS", "reading blob size: %d, data: %d", sizeof(T), sizeof(data));
        size_t size = sizeof(T);
        return nvs_get_blob(handle, key.c_str(), &data, &size);
    }
    
    // restricted to trivially copyable types only!!
    template<typename T>
    esp_err_t write_blob(const std::string &key, const T &data){
        if (!handle) return ESP_ERR_NVS_INVALID_HANDLE;
        ESP_LOGW("NVS", "writing blob size: %d, data: %d", sizeof(T), sizeof(data));
        esp_err_t err = nvs_set_blob(handle, key.c_str(), &data, sizeof(T));
        if (err == ESP_OK) nvs_commit(handle);
        return err;
    }

    // restricted to trivially copyable types only!!
    template<typename T>
    esp_err_t read_vector(const std::string &key, std::vector<T> &vec) {
        if (!handle) return ESP_ERR_NVS_INVALID_HANDLE; 
        size_t length = 0; 

        esp_err_t err = nvs_get_blob(handle, key.c_str(), NULL, &length);
        if (err != ESP_OK) return err;

        ESP_LOGW("NVS", "reading vector length: %d", length);

        vec.resize(length / sizeof(T));
        return nvs_get_blob(handle, key.c_str(), vec.data(), &length);
    }

    // restricted to trivially copyable types only!!
    template<typename T>
    esp_err_t write_vector(const std::string &key, const std::vector<T> &vec) {
        if (!handle) return ESP_ERR_NVS_INVALID_HANDLE; 

        esp_err_t err = nvs_set_blob(handle, key.c_str(), vec.data(), vec.size() * sizeof(T));
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        return err; 
    }

    esp_err_t read_string(const std::string &key, std::string &word);
    esp_err_t write_string(const std::string &key, const std::string &word);

    esp_err_t read_u8(const std::string &key, uint8_t &value);
    esp_err_t write_u8(const std::string &key, const uint8_t &value);

    esp_err_t read_u32(const std::string &key, uint32_t &value);
    esp_err_t write_u32(const std::string &key, const uint32_t &value);

    esp_err_t read_float(const std::string &key, float &value);
    esp_err_t write_float(const std::string &key, const float &value);

    esp_err_t erase_key(const std::string &key);
    esp_err_t erase_all();

private:
    nvs_handle_t handle = 0; 
    std::string nameSpaceName; 

};

#endif //NVSSTORAGE_H