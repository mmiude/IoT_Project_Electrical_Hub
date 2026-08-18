#include <cstdint>

typedef struct smartPlugInfo {
    uint16_t short_addr = 0;
    uint8_t endpoint = 0;
    bool online = false;
    bool is_on = false; 

    bool supports_metering = false;
    bool supports_electrical_measurement = false; 

    uint16_t current_divisor{};
    uint16_t current_multiplier{};
    uint16_t voltage_divisor{};
    uint16_t voltage_multiplier{};
    uint16_t power_divisor{};
    uint16_t power_multiplier{};
    uint32_t summation_divisor{};
    uint32_t summation_multiplier{};

    float active_power{};
    float voltage{}; 
    float current{};
    float summation_kwh{}; 
    
} smartPlug;

