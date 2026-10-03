#include "CloudCommunicationManager.h"

const std::unordered_map<data_type_t, std::string> CloudCommunicationManager::dataTypeToString = {
    {data_type_t::DATA_TYPE_DEVICE_JOIN, "JOIN"},
    {data_type_t::DATA_TYPE_DEVICE_LEFT, "LEFT"},
    {data_type_t::DATA_TYPE_ELEC_PRICE, "PRICE"},
    {data_type_t::DATA_TYPE_ENERGY, "ENERGY"},
    {data_type_t::DATA_TYPE_ONLINE_STATE, "ONLINE"},
    {data_type_t::DATA_TYPE_POWER, "POWER"},
    {data_type_t::DATA_TYPE_PRIORITY, "PRIO"},
    {data_type_t::DATA_TYPE_SET_ON, "STATE"},
    {data_type_t::DATA_TYPE_THRESHOLD_LOW, "THR_LOW"},
    {data_type_t::DATA_TYPE_THRESHOLD_MED, "THR_MED"},
    {data_type_t::DATA_TYPE_VOLTAGE, "VOLTAGE"},
    {data_type_t::DATA_TYPE_CURRENT, "CURRENT"},
    {data_type_t::DATA_TYPE_COMMAND, "COMMAND"},
    {data_type_t::DATA_TYPE_WIFI_ONLINE, "WIFI_ONLINE"},
    {data_type_t::DATA_TYPE_DEVICE_NAME, "DEVICE_NAME"},
    {data_type_t::DATA_TYPE_HUB_ID, "HUB_ID"},
    {data_type_t::DATA_TYPE_WIFI_SSID, "SSID"},
    {data_type_t::DATA_TYPE_WIFI_PW, "PW"}
};

const std::unordered_map<std::string_view, data_type_t> CloudCommunicationManager::stringToDataType = {
    {"JOIN", data_type_t::DATA_TYPE_DEVICE_JOIN},
    {"LEFT", data_type_t::DATA_TYPE_DEVICE_LEFT},
    {"PRICE", data_type_t::DATA_TYPE_ELEC_PRICE},
    {"ENERGY", data_type_t::DATA_TYPE_ENERGY},
    {"ONLINE", data_type_t::DATA_TYPE_ONLINE_STATE},
    {"POWER", data_type_t::DATA_TYPE_POWER},
    {"PRIO", data_type_t::DATA_TYPE_PRIORITY},
    {"STATE", data_type_t::DATA_TYPE_SET_ON},
    {"THR_LOW", data_type_t::DATA_TYPE_THRESHOLD_LOW},
    {"THR_MED", data_type_t::DATA_TYPE_THRESHOLD_MED},
    {"VOLTAGE", data_type_t::DATA_TYPE_VOLTAGE},
    {"CURRENT", data_type_t::DATA_TYPE_CURRENT},
    {"COMMAND", data_type_t::DATA_TYPE_COMMAND},
    {"WIFI_ONLINE", data_type_t::DATA_TYPE_WIFI_ONLINE},
    {"DEVICE_NAME", data_type_t::DATA_TYPE_DEVICE_NAME},
    {"HUB_ID", data_type_t::DATA_TYPE_HUB_ID},
    {"SSID", data_type_t::DATA_TYPE_WIFI_SSID},
    {"PW", data_type_t::DATA_TYPE_WIFI_PW}
};

CloudCommunicationManager::CloudCommunicationManager(std::shared_ptr<Uart> uart, EventGroupHandle_t bits, QueueHandle_t controller_queue, QueueHandle_t cloud_queue, QueueHandle_t ui_queue) : uart(uart), event_bits(bits), controller_q(controller_queue), cloud_q(cloud_queue), ui_q(ui_queue) {
    event_q = uart->get_event_queue(); 

    xTaskCreate(CloudCommunicationManager::runner_tx, "TX_TASK", 4096, this, tskIDLE_PRIORITY + 1, &tx_handle); 
    xTaskCreate(CloudCommunicationManager::runner_rx, "RX_TASK", 4096, this, tskIDLE_PRIORITY + 2, &rx_handle);

}

void CloudCommunicationManager::runner_tx(void *params) {
    auto instance = static_cast<CloudCommunicationManager *> (params); 
    instance->run_tx();
}

void CloudCommunicationManager::runner_rx(void *params) {
    auto instance = static_cast<CloudCommunicationManager *> (params);
    instance->run_rx();
}

void CloudCommunicationManager::run_tx() {

    controller_data data{}; 
    std::string line{};

    while(true) {
        if (xQueueReceive(cloud_q, &data, portMAX_DELAY) == pdPASS) {
            line = convert_controller_data_to_json(data);
            ESP_LOGI("CLOUD COMM", "sending line: %s", line.c_str());
            esp_err_t err = uart->write(line);
            if (err == ESP_OK) ESP_LOGI("CLOUD COMM", "sending successfull");
            else ESP_LOGE("CLOUD COMM", "error while sending UART"); 
            line.clear();
        }
    }
}

void CloudCommunicationManager::run_rx() {

    uart_event_t event;
    std::string line{};
    
    while(true) {
        if (xQueueReceive(event_q, &event, portMAX_DELAY) == pdPASS) {
            if (event.type == UART_DATA) {
                while (uart->read_line(event.size, line) == ESP_OK) {

                    if (line.empty() || line.front() != '{' || line.back() != '}') ESP_LOGW("CLOUD COMM", "invalid data received. no json"); 
                    else {
                        ESP_LOGI("CLOUD COMM", "received json: %s", line.c_str()); 
                        controller_data data = convert_json_to_controller_data(line);
                        ESP_LOGI("CLOUD COMM", "controller data id: 0x%016llx", data.device_id);
                        if (data.type == DATA_TYPE_HUB_ID) xQueueSendToBack(ui_q, &data, 0);
                        else if (data.type == DATA_TYPE_WIFI_ONLINE) xEventGroupSetBits(event_bits, WIFI_ALIVE_BIT); 
                        else xQueueSendToBack(controller_q, &data, 0); 
                    }
                    line.clear(); 
                    event.size = 0;
                } 
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) { 
                ESP_LOGE("CLOUD COMM", "uart rx fifo overflow. resetting");
                uart->flush();
                uart->clear_rx_buffer(); 
                xQueueReset(event_q);
                line.clear();
            }
            else ESP_LOGI("CLOUD COMM", "uart event: %d", event.type); 
        }
    }
}

std::string CloudCommunicationManager::convert_controller_data_to_json(controller_data &data) { 
    if (data.type == DATA_TYPE_DEVICE_JOIN || data.type == DATA_TYPE_DEVICE_LEFT) {
        return std::format("{{\"id\":{},\"type\":\"{}\",\"value\":0}}\n", data.device_id, convert_data_type_to_string(data.type));
    }
    else if (data.type == DATA_TYPE_PRIORITY) {
        return std::format("{{\"id\":{},\"type\":\"{}\",\"value\":{}}}\n", data.device_id, convert_data_type_to_string(data.type), data.data.value_int);
    }
    else if (data.type == DATA_TYPE_SET_ON || data.type == DATA_TYPE_ONLINE_STATE || data.type == DATA_TYPE_WIFI_ONLINE) {
        return std::format("{{\"id\":{},\"type\":\"{}\",\"value\":{}}}\n", data.device_id, convert_data_type_to_string(data.type), data.data.flag);
    }
    else if (data.type == DATA_TYPE_COMMAND) {
        return std::format("{{\"id\":{},\"type\":\"{}\",\"value\":{}}}\n", data.device_id, convert_data_type_to_string(data.type), convert_command_type_to_string(data.data.command));
    }
    else if (data.type == DATA_TYPE_DEVICE_NAME || data.type == DATA_TYPE_HUB_ID || data.type == DATA_TYPE_WIFI_SSID || data.type == DATA_TYPE_WIFI_PW) {
        return std::format("{{\"id\":{},\"type\":\"{}\",\"value\":\"{}\"}}\n", data.device_id, convert_data_type_to_string(data.type), data.data.c_value);
    }
    else return std::format("{{\"id\":{},\"type\":\"{}\",\"value\":{}}}\n", data.device_id, convert_data_type_to_string(data.type), data.data.value);    
}

controller_data CloudCommunicationManager::convert_json_to_controller_data(std::string &line) {
    controller_data ctrl_data{};

    std::string_view id_view = value_extraction(line, "\"id\":");
    std::string_view type_view = value_extraction(line, "\"type\":");
    std::string_view value_view = value_extraction(line, "\"value\":"); 

    if (id_view.empty() || type_view.empty() || value_view.empty()){
        ESP_LOGE("CLOUD COMM", "Failed to extract values from line");
        ctrl_data.type = DATA_TYPE_UNKNOWN;
        return ctrl_data; 
    }

    // convert id 
    std::from_chars(id_view.data(), id_view.data() + id_view.size(), ctrl_data.device_id);

    ctrl_data.type = convert_string_to_data_type(type_view); 

    // depending on data type we convert the values:
    if (ctrl_data.type == DATA_TYPE_DEVICE_LEFT || ctrl_data.type == DATA_TYPE_DEVICE_JOIN || ctrl_data.type == DATA_TYPE_PRIORITY) { // int handling
        std::from_chars(value_view.data(), value_view.data() + value_view.size(), ctrl_data.data.value_int);
    } 
    else if (ctrl_data.type == DATA_TYPE_SET_ON || ctrl_data.type == DATA_TYPE_ONLINE_STATE || ctrl_data.type == DATA_TYPE_WIFI_ONLINE) { // bool handling
        if (value_view == "true" || value_view == "1") ctrl_data.data.flag = true; 
        else ctrl_data.data.flag = false; 
    }
    else if (ctrl_data.type == DATA_TYPE_COMMAND) { // command struct handling
        if (value_view == "ON") ctrl_data.data.command = PLUG_ON; 
        else if (value_view == "OFF") ctrl_data.data.command = PLUG_OFF;
        else if (value_view == "TOGGLE") ctrl_data.data.command = TOGGLE_PLUG; 
        else ESP_LOGE("CLOUD_COMM", "UNKNOWN value_view command type."); 
    }
    else if (ctrl_data.type == DATA_TYPE_DEVICE_NAME || ctrl_data.type == DATA_TYPE_HUB_ID || ctrl_data.type == DATA_TYPE_WIFI_SSID || ctrl_data.type == DATA_TYPE_WIFI_PW) { // string handling -> including null to end of char array
        size_t count = std::min(value_view.size(), (size_t)sizeof(ctrl_data.data.c_value) - 1);
        value_view.copy(ctrl_data.data.c_value, count);
        ctrl_data.data.c_value[count] = '\0';
    }
    else std::from_chars(value_view.data(), value_view.data() + value_view.size(), ctrl_data.data.value); // float handling

    return ctrl_data; 
}

std::string CloudCommunicationManager::convert_data_type_to_string(data_type_t &type) {
    auto it = dataTypeToString.find(type); 
    if (it != dataTypeToString.end()) return it->second; 
    else return "UNKNOWN"; 
}

data_type_t CloudCommunicationManager::convert_string_to_data_type(std::string_view string) {
    auto it = stringToDataType.find(string);
    if (it != stringToDataType.end()) return it->second;
    else return DATA_TYPE_UNKNOWN;
}

std::string_view CloudCommunicationManager::value_extraction(std::string_view line, std::string_view key) {
    auto key_pos = line.find(key);

    if (key_pos == std::string_view::npos) return {};

    auto value_start_pos = key_pos + key.size();

    //skipping starting " if string value
    if (line.at(value_start_pos) == '"') value_start_pos++; 

    //find ending char: must be either : " }
    auto value_end_pos = line.find_first_of(",\"}", value_start_pos);

    if (value_end_pos == std::string_view::npos) return {};

    return line.substr(value_start_pos, (value_end_pos - value_start_pos)); 
}

std::string CloudCommunicationManager::convert_command_type_to_string(commands &command) {
    if (command == PLUG_ON) return "\"ON\"";
    else if (command == PLUG_OFF) return "\"OFF\"";
    else if (command == TOGGLE_PLUG) return "\"TOGGLE\"";
    else {
        ESP_LOGE("CLOUD COMM", "unknown command while converting to string");
        return {}; 
    }
}
