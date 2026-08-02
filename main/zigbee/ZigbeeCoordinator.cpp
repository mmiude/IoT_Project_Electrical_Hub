#include "ZigbeeCoordinator.h"
#include <algorithm>

static const char *TAG = "COORDINATOR"; 

ZigbeeCoordinator::ZigbeeCoordinator(){

    event_queue_t = zigbee_gateway_get_queue();
    ESP_LOGI(TAG, "Start ESP Zigbee Stack");
    xTaskCreate(esp_zigbee_stack_main_task, "ZB_GATEWAY", 4096 * 2, NULL, tskIDLE_PRIORITY + 3, NULL); 
    xTaskCreate(ZigbeeCoordinator::runner, "ZB_COORDINATOR", 4096 * 2, this, tskIDLE_PRIORITY + 2,  &handle);

}

// freeRTOS task methods

void ZigbeeCoordinator::runner(void *params){
    auto instance = static_cast<ZigbeeCoordinator *>(params);
    instance->run(); 
}

void ZigbeeCoordinator::run(){

    ESP_LOGI(TAG, "Starting the coordinator task");
    if (event_queue_t == NULL) ESP_LOGE(TAG, "QUEUE NOT INITIALIZED!");
    ESP_LOGW(TAG, "run() event_queue = %p", event_queue_t);
    zigbee_event event;

    while (true) {
        if (xQueueReceive(event_queue_t, &event, portMAX_DELAY) == pdPASS) {
           // auto it = devices.find(event.ieee_address);  // everytime event is received we first search if the plug already exsists on the  map. 
           // smartPlug *plug = (it != devices.end()) ? &it->second : nullptr; 
           smartPlug *plug = find_smart_plug_with_short_addr(event.short_address); // this is used for every event except joining! 

            if (event.type == ZIGBEE_EVENT_DEVICE_JOINED) { 
                bool known_plug = check_joining_with_ieee(event.data.device_joining.ieee_address, event.short_address); // ieee gives us the absolute truth
                if (!known_plug) {
                    //add new device to both maps.
                    auto [dev_it, devices_inserted] = devices.emplace(event.data.device_joining.ieee_address, smartPlug{
                        .short_addr = event.short_address,
                        .endpoint = event.data.device_joining.endpoint,
                    }); 
                    auto [map_it, devices_map_inserted] = devices_address_map.emplace(event.short_address, event.data.device_joining.ieee_address);
                    if (devices_inserted && devices_map_inserted) ESP_LOGI(TAG, "NEW DEVICE ADDED ON BOTH MAPS. short: 0x%04hx, ieee: 0x%04hx", event.short_address, event.data.device_joining.ieee_address);
                }
            }
            else if (event.type == ZIGBEE_EVENT_DEVICE_NOT_FOUND) {
                if (check_joining_with_ieee(event.data.device_joining.ieee_address, event.short_address)){
                    ESP_LOGI(TAG, "known plug failed to be found...");
                } else ESP_LOGE(TAG, "unkonw device not found. RESET DEVICE!");
            }
            else if (event.type == ZIGBEE_EVENT_BINDING_SUCCESSFUL) { 
                if (plug) {
                    ESP_LOGI(TAG, "Binding successful for plug: 0x%04hx. Sending report, multiplier and divioser request.", event.short_address);
                    esp_zigbee_lock_acquire(portMAX_DELAY);
                    send_configure_reporting(plug->short_addr, plug->endpoint);
                    read_electrical_measurement_multipliers(plug->short_addr, plug->endpoint);
                    read_energy_consumption_multipliers(plug->short_addr, plug->endpoint);
                    esp_zigbee_lock_release();
                }
                else ESP_LOGW(TAG, "binding successful for plug which is not on maps. (0x%04hx)", event.short_address);
            } 
            else if (event.type == ZIGBEE_EVENT_BINDING_ERROR) {
                if (plug) {
                    ESP_LOGW(TAG, "Binding error with a plug added to maps. Please reset. (0x%04hx)", event.short_address); 
                } // need to check if ieee address is found regardless of short address (short address may cahnge....) or do we... need to think about this more... 
                else ESP_LOGE(TAG, "Binding error with unknown plug: 0x%04hx. Please reset the plug to factory settings and try again", event.short_address); 
            }
            else if (event.type == ZIGBEE_EVENT_DEVICE_LEFT) { 
                ESP_LOGI(TAG, "smart plug left: 0x%04hx", event.short_address);
                if (plug) {
                    delete_device_from_both_maps(event.short_address);
                }
                else ESP_LOGW(TAG, "unkonwn devices left");
            }
            else if (event.type == ZIGBEE_EVENT_ONOFF_REPORT) {
                if (plug) {
                    plug->is_on = event.data.is_on ? "ON" : "OFF";
                    ESP_LOGI(TAG, "smart plug: 0x%04hx on/off report (ON/OFF state: %s)", plug->short_addr, event.data.is_on ? "ON" : "OFF");
                }
                else ESP_LOGW(TAG, "on/off report from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_POWER_REPORT) {
                if (plug) {
                    plug->active_power = (float)event.data.raw_power * plug->power_multiplier / plug->power_divisor;
                    ESP_LOGI(TAG, "smart plug: 0x%04hx power: %.2f", plug->short_addr, plug->active_power);
                }
                else ESP_LOGW(TAG, "power report from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_VOLTAGE_REPORT) {
                if (plug) {
                    plug->voltage = (float)event.data.raw_voltage * plug->voltage_multiplier / plug->voltage_divisor;
                    ESP_LOGI(TAG, "smart plug: 0x%04hx voltage: %.2f", plug->short_addr, plug->voltage);
                }
                else ESP_LOGW(TAG, "voltage report from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_CURRENT_REPORT) {
                if (plug) {
                    plug->current = (float)event.data.raw_current * plug->current_multiplier / plug->current_divisor;
                    ESP_LOGI(TAG, "smart plug: 0x%04hx current: %.4f", plug->short_addr, plug->current); 
                }
                else ESP_LOGW(TAG, "current report from unkown smart plug"); 
            }
            else if (event.type == ZIGBEE_EVENT_SUMMATION_REPORT) {
                if (plug) {
                    plug->summation_kwh = (float)event.data.raw_summation * plug->summation_multiplier / plug->summation_divisor;
                    ESP_LOGI(TAG, "smart plug: 0x%04hx summation: %.2f", plug->short_addr, plug->summation_kwh);
                }
                else ESP_LOGW(TAG, "summation report from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_POWER_MULTIPLIER) {
                if (plug) {
                    plug->power_multiplier = event.data.power_multiplier;
                    plug->supports_electrical_measurement = true; 
                }
                else ESP_LOGW(TAG, "power multiplier from unkown smart plug"); 
            }
            else if (event.type == ZIGBEE_EVENT_POWER_DIVISOR) {
                if (plug) {
                    plug->power_divisor = event.data.power_divisor;
                    plug->supports_electrical_measurement = true; 
                }
                else ESP_LOGW(TAG, "power divisor from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_VOLTAGE_MULTIPLIER){
                if (plug) {
                    plug->voltage_multiplier = event.data.voltage_multiplier;
                    plug->supports_electrical_measurement = true; 
                }
                else ESP_LOGW(TAG, "voltage multiplier from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_VOLTAGE_DIVISOR) {
                if (plug) {
                    plug->voltage_divisor = event.data.volgate_divisor;
                    plug->supports_electrical_measurement = true; 
                }
                else ESP_LOGW(TAG, "voltage divisor from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_CURRENT_MULTIPLIER) {
                if (plug) {
                    plug->current_multiplier = event.data.current_multiplier;
                    plug->supports_electrical_measurement = true; 
                }
                else ESP_LOGW(TAG, "current multiplier from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_CURRENT_DIVISOR) {
                if (plug) {
                    plug->current_divisor = event.data.current_divisor;
                    plug->supports_electrical_measurement = true; 
                }
                else ESP_LOGW(TAG, "current divisor from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_SUMMATION_MULTIPLIER) {
                if (plug) {
                    plug->summation_multiplier = event.data.summation_multiplier;
                    plug->supports_metering = true;
                }
                else ESP_LOGW(TAG, "summation multiplier from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_SUMMATION_DIVISOR) {
                if (plug) {
                    plug->summation_divisor = event.data.summation_divisor;
                    plug->supports_metering = true;
                }
                else ESP_LOGW(TAG, "summation divisor form unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_ATTRIBUTE_SUPPORT_ERROR) {
                if (plug) {
                    if (event.data.unsupported_attr == EZB_ZCL_ATTR_METERING_DIVISOR_ID || event.data.unsupported_attr == EZB_ZCL_ATTR_METERING_MULTIPLIER_ID) {
                        plug->supports_metering = false;
                    } 
                    else if (event.data.unsupported_attr == EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_MULTIPLIER_ID || event.data.unsupported_attr == EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_MULTIPLIER_ID || event.data.unsupported_attr == EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_MULTIPLIER_ID) {
                        plug->supports_electrical_measurement = false; 
                    }
                    else ESP_LOGE(TAG, "unkown unsupported attribute from smart plug: 0x%04hx", event.short_address); 
                }
            }
            else ESP_LOGW(TAG, "unknown event type");
        }
    }
}

// public methods. Will be used through controller interface eventually. 

void ZigbeeCoordinator::get_energy_consumption(uint16_t short_addr){
    for (const auto& [key, value] : devices) {
        if (value.supports_electrical_measurement) {
            esp_zigbee_lock_acquire(portMAX_DELAY);
            read_electrical_measurement_values(value.short_addr, value.endpoint); 
            esp_zigbee_lock_release();
        }  
    }
}

void ZigbeeCoordinator::get_electrical_values(uint16_t short_addr){
    for (const auto& [key, value] : devices) {
        if (value.supports_metering) {
            esp_zigbee_lock_acquire(portMAX_DELAY);
            read_energy_consumption_value(value.short_addr, value.endpoint); 
            esp_zigbee_lock_release();
        }
    }
}

int ZigbeeCoordinator::check_device_count(){
    for (const auto& [key, value] : devices) {
        printf("Device: 0x%04hx\n", value.short_addr); 
    }
    printf("address map size: %d\n", devices_address_map.size()); 
    return devices.size(); 
}

void ZigbeeCoordinator::toggle_smart_plug(uint16_t short_addr){
    esp_zigbee_lock_acquire(portMAX_DELAY);
    send_toggle_smart_plug(short_addr, devices[short_addr].endpoint);
    esp_zigbee_lock_release();
}

// private methods 
smartPlug* ZigbeeCoordinator::find_smart_plug_with_short_addr(uint16_t short_addr){ // this we can trust on every other event execept DEVICE_JOINING 
    auto it = devices_address_map.find(short_addr);
    
    if (it != devices_address_map.end()){
        auto plug = devices.find(it->second); 
        return (plug != devices.end()) ? &plug->second : nullptr; 
    } else return nullptr; 
}

bool ZigbeeCoordinator::check_joining_with_ieee(uint64_t ieee_addr, uint16_t short_addr){ // ieee_address can never change. If we want to know for sure if the short address is changed we check it with this. 
    auto it = devices.find(ieee_addr); 
    if (it != devices.end()) {
        if (short_addr == it->second.short_addr) ESP_LOGI(TAG, "device rejoined with the same short address");
        else {
            it->second.short_addr = short_addr;
            ESP_LOGW(TAG, "device rejoined with new short address. Update both maps.");
            update_devices_address_map(ieee_addr, short_addr);
        }
        return true;   
    } return false;
}

void ZigbeeCoordinator::delete_device_from_both_maps(uint16_t short_addr){
    auto it = devices_address_map.find(short_addr);
    if (it != devices_address_map.end()){
        devices.erase(it->second);
    } else ESP_LOGE(TAG, "trying to delete unkonw devices from devices map.");
    devices_address_map.erase(short_addr); 
}

void ZigbeeCoordinator::update_devices_address_map(uint64_t ieee_addr, uint16_t short_addr){
    auto it = std::find_if(devices_address_map.begin(), devices_address_map.end(), [ieee_addr](const auto& pair){
        return pair.second == ieee_addr;
    });

    if (it != devices_address_map.end()) {
        devices_address_map.erase(it);
        devices_address_map[short_addr] = ieee_addr;
        ESP_LOGW(TAG, "address map updated.");
    } else ESP_LOGE(TAG, "ieee address not found from address map (inside update_devices_address_map function.).");
}


ezb_err_t ZigbeeCoordinator::read_electrical_measurement_multipliers(uint16_t dst_addr, uint8_t dst_ep){
    uint16_t metering_attrs[] = {
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_MULTIPLIER_ID,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_DIVISOR_ID,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_DIVISOR_ID,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_MULTIPLIER_ID,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_MULTIPLIER_ID,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_DIVISOR_ID  
    };

    ezb_zcl_read_attr_cmd_t read_attr_cmd = {
        .cmd_ctrl = {
            .dst_addr = {
                .addr_mode = EZB_ADDR_MODE_SHORT,
                .u = {
                    .short_addr = dst_addr,
                }
            },
            .dst_ep = dst_ep,
            .src_ep = ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID,
            .cluster_id = EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT,
            .manuf_code = EZB_ZCL_STD_MANUF_CODE,
            .fc = {
                .manuf_specific = 0,
                .direction = 0,
                .dis_default_rsp = 0,
            },
            .cnf_ctx = {
                .cb = 0,
            },
        },
        .payload = {
            .attr_number = 6,
            .attr_field = metering_attrs,
        },
    };

    ezb_err_t err = ezb_zcl_read_attr_cmd_req(&read_attr_cmd);
    return err == EZB_ERR_NONE ? ESP_OK : ESP_FAIL; 
}

ezb_err_t ZigbeeCoordinator::read_electrical_measurement_values(uint16_t dst_addr, uint8_t dst_ep){
    uint16_t metering_attrs[] = {
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID,
    };

    ezb_zcl_read_attr_cmd_t read_attr_cmd = {
        .cmd_ctrl = {
            .dst_addr = {
                .addr_mode = EZB_ADDR_MODE_SHORT,
                .u = {
                    .short_addr = dst_addr,
                }
            },
            .dst_ep = dst_ep,
            .src_ep = ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID,
            .cluster_id = EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT,
            .manuf_code = EZB_ZCL_STD_MANUF_CODE,
            .fc = {
                .manuf_specific = 0,
                .direction = 0,
                .dis_default_rsp = 0,
            },
            .cnf_ctx = {
                .cb = 0,
            },
        },
        .payload = {
            .attr_number = 3,
            .attr_field = metering_attrs,
        },
    };

    ezb_err_t err = ezb_zcl_read_attr_cmd_req(&read_attr_cmd);
    return err == EZB_ERR_NONE ? ESP_OK : ESP_FAIL; 
}

ezb_err_t ZigbeeCoordinator::read_energy_consumption_multipliers(uint16_t dst_addr, uint8_t dst_ep){
    uint16_t metering_attrs[] = {
        EZB_ZCL_ATTR_METERING_MULTIPLIER_ID,                   
        EZB_ZCL_ATTR_METERING_DIVISOR_ID, 
    };

    ezb_zcl_read_attr_cmd_t read_attr_cmd = {
        .cmd_ctrl = {
            .dst_addr = {
                .addr_mode = EZB_ADDR_MODE_SHORT,
                .u = {
                    .short_addr = dst_addr,
                }
            },
            .dst_ep = dst_ep,
            .src_ep = ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID,
            .cluster_id = EZB_ZCL_CLUSTER_ID_METERING,
            .manuf_code = EZB_ZCL_STD_MANUF_CODE,
            .fc = {
                .manuf_specific = 0,
                .direction = 0,
                .dis_default_rsp = 0,
            },
            .cnf_ctx = {
                .cb = 0,
            },
        },
        .payload = {
            .attr_number = 2,
            .attr_field = metering_attrs,
        },
    };

    ezb_err_t err = ezb_zcl_read_attr_cmd_req(&read_attr_cmd);
    return err == EZB_ERR_NONE ? ESP_OK : ESP_FAIL; 
}

ezb_err_t ZigbeeCoordinator::read_energy_consumption_value(uint16_t dst_addr, uint8_t dst_ep){
    uint16_t metering_attrs[] = {EZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID};

    ezb_zcl_read_attr_cmd_t read_attr_cmd = {
        .cmd_ctrl = {
            .dst_addr = {
                .addr_mode = EZB_ADDR_MODE_SHORT,
                .u = {
                    .short_addr = dst_addr,
                }
            },
            .dst_ep = dst_ep,
            .src_ep = ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID,
            .cluster_id = EZB_ZCL_CLUSTER_ID_METERING,
            .manuf_code = EZB_ZCL_STD_MANUF_CODE,
            .fc = {
                .manuf_specific = 0,
                .direction = 0,
                .dis_default_rsp = 0,
            },
            .cnf_ctx = {
                .cb = 0,
            },
        },
        .payload = {
            .attr_number = 1,
            .attr_field = metering_attrs,
        },
    };

    ezb_err_t err = ezb_zcl_read_attr_cmd_req(&read_attr_cmd);
    return err == EZB_ERR_NONE ? ESP_OK : ESP_FAIL; 
}

esp_err_t ZigbeeCoordinator::read_plug_on_off_state(uint16_t dst_addr, uint8_t dst_ep){
    uint16_t on_off_attrs[] = {EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID};

    ezb_zcl_read_attr_cmd_t read_attr_cmd = {
        .cmd_ctrl = {
            .dst_addr = {
                .addr_mode = EZB_ADDR_MODE_SHORT, 
                .u = {
                    .short_addr = dst_addr,
                }
            },
            .dst_ep = dst_ep,
            .src_ep = ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID,
            .cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF,
            .manuf_code = EZB_ZCL_STD_MANUF_CODE,
            .fc = {
                .manuf_specific = 0,
                .direction = 0,
                .dis_default_rsp = 0,
            },
            .cnf_ctx = {
                .cb = 0,
            },
        },
        .payload = {
            .attr_number = 1,
            .attr_field = on_off_attrs,
        }
    };

    ezb_err_t err = ezb_zcl_read_attr_cmd_req(&read_attr_cmd);
    return err == EZB_ERR_NONE ? ESP_OK : ESP_FAIL; 
}

esp_err_t ZigbeeCoordinator::send_toggle_smart_plug(uint16_t dst_addr, uint8_t dst_ep){
    ezb_zcl_on_off_cmd_t toggle_cmd = {
        .cmd_ctrl = {
            .dst_addr = {
                .addr_mode = EZB_ADDR_MODE_SHORT,
                .u = {
                    .short_addr = dst_addr,
                }
            },
            .dst_ep = dst_ep,
            .src_ep = ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID,
            .dis_default_rsp = false,
            .cnf_ctx = {
                .cb = 0,
            },
        }
    };

    ezb_err_t err = ezb_zcl_on_off_toggle_cmd_req(&toggle_cmd);
    ESP_LOGI(TAG, "Toggle command result: 0x%04x", err);
    
    return err == EZB_ERR_NONE ? ESP_OK : ESP_FAIL; 
}

esp_err_t ZigbeeCoordinator::send_configure_reporting(uint16_t dst_addr, uint8_t dst_ep){
    // ON/OFF cluster
    // uint8_t onoff_change = 0;
    ezb_zcl_config_report_record_t onoff_records[] = {
        {
            .direction = EZB_ZCL_REPORTING_SEND,
            .attr_id = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
            .client = {
                .attr_type = EZB_ZCL_ATTR_TYPE_BOOL,
                .min_interval = 0,
                .max_interval = 43200,
                .reportable_change = { .u8 = 1},
            },
        },
    };
    ezb_zcl_config_report_cmd_t onoff_cmd = {    
        .cmd_ctrl = {
            .dst_addr = {
                .addr_mode = EZB_ADDR_MODE_SHORT,
                .u = {
                    .short_addr =  dst_addr
                }
            },
            .dst_ep = dst_ep,
            .src_ep = ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID,
            .cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF,
            .manuf_code = EZB_ZCL_STD_MANUF_CODE,
            .fc = {
                .manuf_specific = 0,
                .direction = 0,
                .dis_default_rsp = 0,
            },
        },
        .payload = {
            .record_number = 1,
            .record_field = onoff_records,
        }
    };
    ezb_err_t err = ezb_zcl_config_report_cmd_req(&onoff_cmd);
    ESP_LOGI(TAG, "Configure reporting ON/OFF result: 0x%04x", err);

    vTaskDelay(pdMS_TO_TICKS(100)); 

    return ESP_OK; 
}

    //binding methods 
/*ezb_err_t ZigbeeCoordinator::zdo_find_smart_plug_device(uint16_t dst_addr){
    ezb_err_t ret             = EZB_ERR_FAIL;
    uint16_t  cluster_list[] = {EZB_ZCL_CLUSTER_ID_ON_OFF};

    ezb_zdo_match_desc_req_t req = {
        .dst_nwk_addr = dst_addr,
        .field =
            {
                .nwk_addr_of_interest = dst_addr,
                .profile_id           = EZB_AF_HA_PROFILE_ID,
                .num_in_clusters      = sizeof(cluster_list) / sizeof(cluster_list[0]),
                .num_out_clusters     = 0,
                .cluster_list         = cluster_list,
            },
        .cb       = zdo_find_smart_plug_device_result,
        .user_ctx = this,
    };
    ret = ezb_zdo_match_desc_req(&req);
    if (ret == EZB_ERR_NONE) {
        ESP_LOGI(TAG, "Attempt to find smart_plug device");
    } else {
        ESP_LOGE(TAG, "Failed to find smart_plug device with error(0x%04x)", ret);
    }
    return ret;
}

void ZigbeeCoordinator::zdo_find_smart_plug_device_result(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx){
    assert(result);
    if (result->error == EZB_ERR_NONE) {
        if (result->rsp && result->rsp->status == EZB_ZDP_STATUS_SUCCESS && result->rsp->match_length > 0 &&
            result->rsp->match_list) {
            for (size_t i = 0; i < result->rsp->match_length; i++) {
                ESP_LOGW(TAG, "possible id: %d", result->rsp->match_list[i]);
                static_cast<ZigbeeCoordinator*>(user_ctx)->devices[result->rsp->nwk_addr_of_interest].endpoint = result->rsp->match_list[i];
                static_cast<ZigbeeCoordinator*>(user_ctx)->zdo_bind_smart_plug_device(result->rsp->nwk_addr_of_interest, result->rsp->match_list[i]);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to find smart plug device in the network with error(0x%04x)", result->error);
    }
}

ezb_err_t ZigbeeCoordinator::zdo_bind_smart_plug_device(uint16_t dst_short_addr, uint8_t dst_ep){
    ezb_err_t          ret      = EZB_ERR_FAIL;
    ezb_zdo_bind_req_t bind_req = {
        .dst_nwk_addr = dst_short_addr,
        .field =
            {
                .src_ep        = dst_ep, 
                .cluster_id    = EZB_ZCL_CLUSTER_ID_ON_OFF,
                .dst_addr_mode = EZB_ADDR_MODE_EXT,
                .dst_ep        = ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID,
            },
        .cb       = zdo_bind_smart_plug_result,
        .user_ctx = this,
    };
    ezb_address_extended_by_short(dst_short_addr, &bind_req.field.src_addr);
    ezb_nwk_get_extended_address(&bind_req.field.dst_addr.extended_addr);

    ret = ezb_zdo_bind_req(&bind_req);

    if (ret == EZB_ERR_NONE) {
        ESP_LOGI(TAG, "Attempt to bind smart plug device (short address: 0x%04hx)", dst_short_addr);
    } else {
        ESP_LOGE(TAG, "Failed to bind smart plug device (short address: 0x%04hx) with error(0x%04x)", dst_short_addr, ret);
    }
    return ret;
}

void ZigbeeCoordinator::zdo_bind_smart_plug_result(const ezb_zdp_bind_req_result_t *result, void *user_ctx){
    assert(result);
    uint16_t short_address = static_cast<ZigbeeCoordinator*>(user_ctx)->binding_short_addr;
    auto instance = static_cast<ZigbeeCoordinator*>(user_ctx); 

    if (result->error == EZB_ERR_NONE) {
        if (result->rsp && result->rsp->status == EZB_ZDP_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Bound smart plug device successfully");
            esp_zigbee_lock_acquire(portMAX_DELAY);
            instance->send_configure_reporting(instance->devices[short_address].short_addr, instance->devices[short_address].endpoint);
            instance->read_electrical_measurement_multipliers(instance->devices[short_address].short_addr, instance->devices[short_address].endpoint);
            instance->read_energy_consumption_multipliers(instance->devices[short_address].short_addr, instance->devices[short_address].endpoint);
            esp_zigbee_lock_release(); 

        } else {
            ESP_LOGE(TAG, "Failed to bind smart plug device with status (0x%02x)", result->rsp->status);
        }
    } else {
        ESP_LOGE(TAG, "Failed to bind smart plug device with error (0x%04x)", result->error);
    }
}*/