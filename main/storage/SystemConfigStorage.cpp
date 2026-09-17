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

esp_err_t SystemConfigStorage::get_threshold_levels(float &low, float &med) {
    esp_err_t err = storage.read_blob(low_thres_key, low);
    if (err != ESP_OK) return err;
    return storage.read_blob(med_thres_key, med);
}

esp_err_t SystemConfigStorage::erase_wifi_info(){
    esp_err_t err = storage.erase_key(ssid_key);
    if (err != ESP_OK) {
        ESP_LOGE("CONFIG_STORAGE", "error while erasing ssid from NVS.");
        return err;
    }
    return storage.erase_key(pwd_key);
} 

esp_err_t SystemConfigStorage::erase_low_threshold(){
    return storage.erase_key(low_thres_key);
}

esp_err_t SystemConfigStorage::erase_med_threshold(){
    return storage.erase_key(med_thres_key);
}

esp_err_t SystemConfigStorage::erase_all_system_config_info(){
    return storage.erase_all();
}