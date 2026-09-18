#ifndef LED_H
#define LED_H

#include "esp_log.h"
#include "driver/gpio.h"

class Led {
public:
    Led(gpio_num_t green_pin, gpio_num_t yellow_pin, gpio_num_t red_pin);
    ~Led() = default; 

    void blink_green(); 
    void blink_yellow();
    void blink_red();

private:
    gpio_num_t led_green;
    gpio_num_t led_yellow;
    gpio_num_t led_red; 

    uint8_t green_state;
    uint8_t yellow_state;
    uint8_t red_state; 
};

#endif //LED_H