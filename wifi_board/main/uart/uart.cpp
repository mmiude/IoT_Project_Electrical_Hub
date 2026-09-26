#include "uart.h"

Uart::Uart(uart_port_t _uart_num, int tx_io_pin, int rx_io_pin,
    QueueHandle_t _rx_queue, QueueHandle_t _tx_queue)
: uart_num(_uart_num), rx_queue(_rx_queue), tx_queue(_tx_queue)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(uart_num, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_io_pin, rx_io_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(uart_task, "UART_TASK", 2048, static_cast<void*>(this), tskIDLE_PRIORITY + 1, NULL);
}

void Uart::uart_task(void *param)
{
    char *pcName = pcTaskGetName(NULL);
    ESP_LOGI(pcName, "Started");

    auto instance = static_cast<Uart*>(param);

    controller_data ctrl_data = {};
    while (true) {
        if (xQueueReceive(instance->tx_queue, &ctrl_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            int tx_bytes = uart_write_bytes(instance->uart_num, &ctrl_data, sizeof(controller_data));
            ESP_LOGI(pcName, "Wrote %d bytes to uart", tx_bytes);
        }
        
        int rx_bytes = uart_read_bytes(instance->uart_num,
            &ctrl_data,
            sizeof(controller_data),
            pdMS_TO_TICKS(1000)
        );
        if (rx_bytes >= 0) {
            ESP_LOGI(pcName, "Read %d bytes from uart", rx_bytes);
            if (rx_bytes == sizeof(controller_data)
                && xQueueSendToBack(instance->rx_queue, &ctrl_data, 0) == pdTRUE
            ) {
                ESP_LOGI(pcName, "Data passed to queue");
            }
        }

    }
}