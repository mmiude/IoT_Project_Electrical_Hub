#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "Uart.h"
#include <memory>

class CloudCommunication {
public:
    CloudCommunication(std::shared_ptr<Uart> uart, QueueHandle_t controller_queue, QueueHandle_t cloud_queue);

private: 
    QueueHandle_t controller_q;
    QueueHandle_t cloud_q; 
};