#ifndef SYSTEMCONFIGSTORAGE_H
#define SYSTEMCONFIGSTORAGE_H

#include <string>
#include "NvsStorage.h"

// this is used to save system configuration info including wi-fi ssid and pwd and threshold levels. 

class SystemConfigStorage {
public:
    SystemConfigStorage(std::string name_space = "sys_conf", std::string wifi_ssid_k = "ssid_key", std::string wifi_pwd_k = "pwd_key", std::string threshold_key_med = "t_med_key", std::string threshold_key_low = "t_low_key");
    ~SystemConfigStorage = default;

    esp_err_t save_wifi_info(std::string &ssid, std::string &pwd);
    esp_err_t save_low_threshold(float low_threshold);
    esp_err_t save_med_threshold(float med_threshold);

    esp_err_t get_wifi_info(std::string &ssid, std::string &pwd);
    esp_err_t get_low_threshold(float &low_threshold); 
    esp_err_t get_med_threshold(float &med_threshold); 

private:
    NvsStorage storage;
    std::string ssid_key;
    std::string pwd_key;
    std::string threshold_key;
};
#endif //SYSTEMCONFIGSTORAGE_H