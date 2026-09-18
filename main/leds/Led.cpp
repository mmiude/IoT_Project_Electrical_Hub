#include "Led.h"

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

    green_state = 0;
    yellow_state = 0;
    red_state = 0;

}

void Led::update(int staten) {
    //if zigee network is up -> green on red off

    //if zigbee network is open -> blink yellow

    //if zigbee network is close -> yello off 

    //if no activity -> red on green off
}

void Led::blink_green() {
    green_state = !green_state;
    gpio_set_level(led_green, green_state);
}

void Led::blink_yellow() {
    yellow_state = !yellow_state;
    gpio_set_level(led_yellow, yellow_state);
}

void Led::blink_red() {
    red_state = !red_state;
    gpio_set_level(led_red, red_state); 
}