#include "Led.h"
#include "HubControllerEnums.h"

Led::Led(gpio_num_t green_pin, gpio_num_t yellow_pin, gpio_num_t red_pin) : led_green(green_pin), led_yellow(yellow_pin), led_red(red_pin) {

    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << led_green) | (1ULL << led_yellow) | (1ULL << led_red),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&led_conf); 

    if (err != ESP_OK) ESP_LOGE("LEDS", "error while initializing leds."); 

    yellow_state = 0;

    timer_handle = xTimerCreate("BLINK_TIMER", pdMS_TO_TICKS(500), pdTRUE, this, blinkTimerCallback);

}

void Led::blinkTimerCallback(TimerHandle_t xTimer) {
    auto instance = static_cast<Led *>(pvTimerGetTimerID(xTimer));
    instance->yellow_state = !instance->yellow_state;
    gpio_set_level(instance->led_yellow, instance->yellow_state);
}

void Led::update(int state) {

    switch(state) {
        case Z_NETWORK_OPEN:
            set_green(1);
            xTimerStart(timer_handle, 0);
            break;
        case Z_NETWORK_CLOSE:
            xTimerStop(timer_handle, 0);
            break;
        case Z_NETWORK_DOWN:
            set_red(1);
            set_green(0);
            break;
        default:
            ESP_LOGE("LEDS", "unkown update state.");
            break; 
    }
}

void Led::set_green(int state) {
    gpio_set_level(led_green, state);
}

void Led::set_red(int state) {
    gpio_set_level(led_red, state); 
}