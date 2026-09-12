#include "NvsStorage.h"

NvsStorage::NvsStorage(const std::string &nameSpace_n) : nameSpaceName(nameSpace_n) {
    esp_err_t err = nvs_open(nameSpaceName.c_str(), NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "failed to open: %s", nameSpaceName); 
        handle = 0; 
    }
}

NvsStorage::~NvsStorage() {
    if (handle) {
        nvs_commit(handle);
        nvs_close(handle);
    }
}

esp_err_t NvsStorage::read_string(const std::string &key, std::string &word) {
    if (!handle) return ESP_ERR_NVS_INVALID_HANDLE;
    size_t required_size = 0; 
    esp_err_t err = nvs_get_str(handle, key.c_str(), NULL, &required_size);

    if (err != ESP_OK ) {
        ESP_LOGE("NVS", "Error (%s) reading string size!", esp_err_to_name(err));
        return err;
    } 

    word.resize(required_size - 1);
    return nvs_get_str(handle, key.c_str(), &word[0], &required_size);
}

esp_err_t NvsStorage::write_string(const std::string &key, const std::string &word) {
    if (!handle) return ESP_ERR_NVS_INVALID_HANDLE; 
    esp_err_t err = nvs_set_str(handle, key.c_str(), word.c_str());
    if (err == ESP_OK) nvs_commit(handle);
    return err;
}

esp_err_t NvsStorage::read_u8(const std::string &key, uint8_t &value) {
    if (!handle) return ESP_ERR_NVS_INVALID_HANDLE; 
    return nvs_get_u8(handle, key.c_str(), &value);
}

esp_err_t NvsStorage::write_u8(const std::string &key, const uint8_t &value) {
    if (!handle) return ESP_ERR_NVS_INVALID_HANDLE;
    esp_err_t err = nvs_set_u8(handle, key.c_str(), value);
    if (err == ESP_OK) nvs_commit(handle);
    return err;
}

esp_err_t NvsStorage::read_u32(const std::string &key, uint32_t &value) {
    if (!handle) return ESP_ERR_NVS_INVALID_HANDLE;
    return nvs_get_u32(handle, key.c_str(), &value);
}

esp_err_t NvsStorage::write_u32(const std::string &key, const uint32_t &value) {
    if (!handle) return ESP_ERR_NVS_INVALID_HANDLE;
    esp_err_t err = nvs_set_u32(handle, key.c_str(), value);
    if (err == ESP_OK) nvs_commit(handle);
    return err; 
}

esp_err_t NvsStorage::erase_key(const std::string &key) {
    if (!handle) return ESP_ERR_NVS_INVALID_HANDLE;
    esp_err_t err = nvs_erase_key(handle, key.c_str());
    if (err == ESP_OK) nvs_commit(handle);
    return err;
}

esp_err_t NvsStorage::erase_all() {
    if (!handle) return ESP_ERR_NVS_INVALID_HANDLE;
    esp_err_t err = nvs_erase_all(handle); 
    if (err == ESP_OK) nvs_commit(handle);
    return err; 
}

