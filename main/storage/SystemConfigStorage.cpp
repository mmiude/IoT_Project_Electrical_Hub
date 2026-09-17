#include "SystemConfigStorage.h"

SystemConfigStorage::SystemConfigStorage(std::string name_space, std::string wifi_ssid_k, std::string wifi_pwd_k, std::string threshold_key_med, std::string threshold_key_low) :
    storage(name_space), ssid_key(wifi_ssid_k), pwd_key(wifi_pwd_k), med_thres_key(threshold_key_med), low_thres_key(threshold_key_low) {

}

esp_err_t SystemConfigStorage::save_wifi_info(std::string &ssid, std::string &pwd){
    esp_err_t err = storage.write_string(ssid_key, ssid); 
    if (err != ESP_OK) {
        ESP_LOGE("CONFIG_STORAGE", "error while saving ssid into nvs.");
        return err;
    } 
    return storage.write_string(pwd_key, pwd);
}

esp_err_t SystemConfigStorage::save_low_threshold(float low_threshold){
    return storage.write_blob(low_thres_key, low_threshold);
}

esp_err_t SystemConfigStorage::save_med_threshold(float med_threshold){
    return storage.write_blob(med_thres_key, med_threshold);
}

esp_err_t SystemConfigStorage::get_wifi_info(std::string &ssid, std::string &pwd){
    esp_err_t err = storage.read_string(ssid_key, ssid);
    if (err != ESP_OK) {
        ESP_LOGE("CONFIG_STORAGE", "error while reading ssid from nvs.");
        return err;
    }
    return storage.read_string(pwd_key, pwd);
}

esp_err_t SystemConfigStorage::get_low_threshold(float &low_threshold){
    return storage.read_blob(low_thres_key, low_threshold);
}

esp_err_t SystemConfigStorage::get_med_threshold(float &med_threshold){
    return storage.read_blob(low_thres_key, med_threshold);
}