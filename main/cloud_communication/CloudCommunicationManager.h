#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "HubControllerEnums.h"
#include "Uart.h"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class CloudCommunicationManager {
public:
    CloudCommunicationManager(std::shared_ptr<Uart> uart, EventGroupHandle_t bits, QueueHandle_t controller_queue, QueueHandle_t cloud_queue, QueueHandle_t ui_queue);

private: 
    static void runner_tx(void *params);
    static void runner_rx(void *params);
    void run_tx();
    void run_rx();

    static const std::unordered_map<data_type_t, std::string> dataTypeToString; 
    static const std::unordered_map<std::string_view, data_type_t> stringToDataType;

    std::shared_ptr<Uart> uart;

    EventGroupHandle_t event_bits;
    QueueHandle_t controller_q;
    QueueHandle_t cloud_q; 
    QueueHandle_t ui_q;
    QueueHandle_t event_q;

    TaskHandle_t tx_handle;
    TaskHandle_t rx_handle; 

    std::string convert_controller_data_to_json(controller_data &data);
    controller_data convert_json_to_controller_data(std::string &line);

    std::string convert_data_type_to_string(data_type_t &type);
    data_type_t convert_string_to_data_type(std::string_view string); 
    std::string convert_command_type_to_string(commands &command);

    std::string_view value_extraction(std::string_view line, std::string_view key);

};