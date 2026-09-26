#ifndef UART_H
#define UART_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"
// #include "esp_crc.h"

#include "HubControllerEnums.h"

// typedef struct __attribute__((packed)) {
//     uint16_t header;
//     controller_data data;
//     uint16_t crc;
// } t_uart_packet;

// #define FRAME_HEADER 0xAA55

// #define UART_PORT_NUM UART_NUM_1
#define UART_BUF_SIZE 1024

class Uart
{
private:
    uart_port_t uart_num;
    
    QueueHandle_t rx_queue;
    QueueHandle_t tx_queue;

    // t_uart_packet prepare_packet(controller_data data);
    // bool verify_packet(const controller_data *data);

    static void uart_task(void *param);
public:
    Uart(uart_port_t _uart_num, int tx_io_pin, int rx_io_pin,
        QueueHandle_t _rx_queue, QueueHandle_t _tx_queue);
};

#endif