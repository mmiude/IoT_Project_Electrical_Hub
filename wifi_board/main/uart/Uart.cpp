#include "Uart.h"

#include <algorithm>

Uart::Uart(uart_port_t uart_port, int tx_pin, int rx_pin, QueueHandle_t event_q, int baudRate) : uart_port(uart_port), tx_pin(tx_pin), rx_pin(rx_pin), uart_event_queue(event_q), baud_rate(baudRate) {

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1, 
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    uart_param_config(uart_port, &uart_config); 
    uart_set_pin(uart_port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    esp_err_t err = uart_driver_install(uart_port, 1024, 1024, 10, &uart_event_queue, 0);

    if (err != ESP_OK) ESP_LOGE("UART", "error while initializing uart"); 

    rx_buffer = {};

}

esp_err_t Uart::write(std::string &line) {
    int bytes_written = uart_write_bytes(uart_port, line.c_str(), line.size()); // todo check this
    if (bytes_written > 0) return ESP_OK; 
    return ESP_FAIL; 
}

esp_err_t Uart::read_line(size_t event_size, std::string &line) {

    if (event_size > 0) {
        std::vector<char> temp_buffer(event_size);

        int read_bytes = uart_read_bytes(uart_port, temp_buffer.data(), event_size, 0);

        if (read_bytes > 0) rx_buffer.append(temp_buffer.data(), read_bytes);  
    }
    size_t pos = rx_buffer.find('\n'); 

    if (pos != std::string::npos) {
        line = rx_buffer.substr(0, pos);
        rx_buffer.erase(0, pos + 1);  

        if (!line.empty() && line.back() == '\r') line.pop_back(); 
        return ESP_OK;
    }
    return ESP_ERR_NOT_FINISHED;
}

// esp_err_t Uart::read_line(size_t event_size, std::string &line) {

//     //char temp_buffer[256]; // and here as well 
//     std::vector<char> temp_buffer(event_size);

//     int read_bytes = uart_read_bytes(uart_port, temp_buffer.data(), event_size, 0);

//     for (int i = 0; i < read_bytes; i++){
//         char byte = temp_buffer[i];
//         if (byte == '\n') {
//             return ESP_OK;
//         }
//         else if (byte == '\r') ESP_LOGI("UART", "skipping \r"); 
//         else line += byte; 
//     }

//     return ESP_ERR_NOT_FINISHED;
// }

esp_err_t Uart::flush() {
    return uart_flush(uart_port); 
}

QueueHandle_t Uart::get_event_queue() {
    return uart_event_queue; 
}

void Uart::clear_rx_buffer() {
    rx_buffer.clear(); 
}