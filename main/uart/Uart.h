#ifndef UART_H
#define UART_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h" 
#include "esp_log.h"

class Uart {
public:
    Uart(uart_port_t uart_port, int tx_pin, int rx_pin, int baudRate = 115200);

    esp_err_t write_bytes();
    esp_err_t read_bytes();
    esp_err_t read_line(); 
    esp_err_t flush(); 
    QueueHandle_t get_event_queue();

private:
    uart_port_t uart_port;
    QueueHandle_t uart_event_queue(); 
    int baude_rate; 
    int tx_pin; 
    int rx_pin; 
};

#endif //UART_H 