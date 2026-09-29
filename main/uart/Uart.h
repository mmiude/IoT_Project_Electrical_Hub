#ifndef UART_H
#define UART_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h" 
#include "esp_log.h"

#include <string>
#include <vector>

class Uart {
public:
    Uart(uart_port_t uart_port, int tx_pin, int rx_pin, QueueHandle_t event_q, int baudRate = 115200);

    esp_err_t write(std::string &line);
    esp_err_t read_line(size_t event_size, std::string &line); 
    esp_err_t flush(); 
    QueueHandle_t get_event_queue();

private:
    uart_port_t uart_port;
    int tx_pin; 
    int rx_pin; 
    QueueHandle_t uart_event_queue; 
    int baud_rate; 
    
};

#endif //UART_H 