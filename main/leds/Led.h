#ifndef LED_H
#define LED_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "Observer.h"

class Led : public Observer {
public:
    Led(gpio_num_t green_pin, gpio_num_t yellow_pin, gpio_num_t red_pin);
    ~Led() = default; 

    void update(int state) override;

private:
    gpio_num_t led_green;
    gpio_num_t led_yellow;
    gpio_num_t led_red; 

    uint8_t yellow_state; 

    void set_green(int state);
    void set_red(int state);
    void set_yellow(int state);

    static void blinkTimerCallback(TimerHandle_t xTimer); 

    TimerHandle_t timer_handle; 
};

#endif //LED_H