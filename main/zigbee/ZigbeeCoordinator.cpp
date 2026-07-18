#include "ZigbeeCoordinator.h"

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
            // first check if we are handling known smart plug -> we get pointer to it so we don't need to look it up from the std::map many times
            auto it = devices.find(event.short_address);
            smartPlug *plug = (it != devices.end()) ? &it->second : nullptr; 

            if (event.type == ZIGBEE_EVENT_DEVICE_JOINED) {
                if (plug){
                    ESP_LOGI(TAG, "smart plug rejoined: 0x%04hx", event.short_address);
                }
                else {
                    ESP_LOGI(TAG, "new smart plug joined: 0x%04hx", event.short_address);
                    binding_short_addr = event.short_address; 
                    
                    esp_zigbee_lock_acquire(portMAX_DELAY);
                    if (zdo_find_smart_plug_device(event.short_address) == ESP_OK) devices[event.short_address].short_addr = event.short_address;
                    else {
                        vTaskDelay(pdMS_TO_TICKS(500)); //if first try fails we take a breather and try again. not the prettiest solution here... 
                        if (zdo_find_smart_plug_device(event.short_address) == ESP_OK) devices[event.short_address].short_addr = event.short_address;
                        else ESP_LOGE(TAG, "failed to find smart plug. Pls reset the plug to factory settings and try again. No devices added to coordinator");
                    }
                    esp_zigbee_lock_release();
                    //plug = &devices[event.short_address]; 
                }
            } 
            else if (event.type == ZIGBEE_EVENT_DEVICE_LEFT) {
                ESP_LOGI(TAG, "smart plug left: 0x%04hx", event.short_address);
                if (plug) {
                    devices.erase(event.short_address); 
                    ESP_LOGI(TAG, "smart plug deleted from devices.");
                    plug = nullptr; // so we don't end up with dangling pointer 
                }
                else ESP_LOGW(TAG, "unkonwn devices left");
            }
            else if (event.type == ZIGBEE_EVENT_ONOFF_REPORT) {
                if (plug) {
                    plug->is_on = event.data.is_on ? "ON" : "OFF";
                    ESP_LOGI(TAG, "smart plug: 0x%04hx on/off report (ON/OFF state: %s)", event.short_address, event.data.is_on ? "ON" : "OFF");
                }
                else ESP_LOGW(TAG, "on/off report from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_POWER_REPORT) {
                if (plug) {
                    plug->active_power = (float)event.data.raw_power * plug->power_multiplier / plug->power_divisor;
                    ESP_LOGI(TAG, "smart plug: 0x%04hx power: %.2f", event.short_address, plug->active_power);
                }
                else ESP_LOGW(TAG, "power report from unkown smart plug");
                //devices[event.short_address].active_power = (float)event.data.raw_power * devices[event.short_address].power_multiplier / devices[event.short_address].power_divisor; 
                //ESP_LOGI(TAG, "smart plug: 0x%04hx power: %.2f", event.short_address, devices[event.short_address].active_power);
            }
            else if (event.type == ZIGBEE_EVENT_VOLTAGE_REPORT) {
                if (plug) {
                    plug->voltage = (float)event.data.raw_voltage * plug->voltage_multiplier / plug->voltage_divisor;
                    ESP_LOGI(TAG, "smart plug: 0x%04hx voltage: %.2f", event.short_address, plug->voltage);
                }
                else ESP_LOGW(TAG, "voltage report from unkown smart plug");
                //devices[event.short_address].voltage = (float)event.data.raw_voltage * devices[event.short_address].voltage_multiplier / devices[event.short_address].voltage_divisor;
                //ESP_LOGI(TAG, "smart plug: 0x%04hx voltage: %.2f", event.short_address, devices[event.short_address].voltage );
            }
            else if (event.type == ZIGBEE_EVENT_CURRENT_REPORT) {
                if (plug) {
                    plug->current = (float)event.data.raw_current * plug->current_multiplier / plug->current_divisor;
                    ESP_LOGI(TAG, "smart plug: 0x%04hx current: %.4f", event.short_address, plug->current); 
                }
                else ESP_LOGW(TAG, "current report from unkown smart plug"); 
                //devices[event.short_address].current = (float)event.data.raw_current * devices[event.short_address].current_multiplier / devices[event.short_address].current_divisor;
                //ESP_LOGI(TAG, "smart plug: 0x%04hx current: %.2f", event.short_address, devices[event.short_address].current);
            }
            else if (event.type == ZIGBEE_EVENT_SUMMATION_REPORT) {
                if (plug) {
                    plug->summation_kwh = (float)event.data.raw_summation * plug->summation_multiplier / plug->summation_divisor;
                    ESP_LOGI(TAG, "smart plug: 0x%04hx summation: %.2f", event.short_address, plug->summation_kwh);
                }
                else ESP_LOGW(TAG, "summation report from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_POWER_MULTIPLIER) {
                if (plug) {
                    plug->power_multiplier = event.data.power_multiplier;
                }
                else ESP_LOGW(TAG, "power multiplier from unkown smart plug"); 
            }
            else if (event.type == ZIGBEE_EVENT_POWER_DIVISOR) {
                if (plug) {
                    plug->power_divisor = event.data.power_divisor;
                }
                else ESP_LOGW(TAG, "power divisor from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_VOLTAGE_MULTIPLIER){
                if (plug) {
                    plug->voltage_multiplier = event.data.voltage_multiplier;
                }
                else ESP_LOGW(TAG, "voltage multiplier from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_VOLTAGE_DIVISOR) {
                if (plug) {
                    plug->voltage_divisor = event.data.volgate_divisor;
                }
                else ESP_LOGW(TAG, "voltage divisor from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_CURRENT_MULTIPLIER) {
                if (plug) {
                    plug->current_multiplier = event.data.current_multiplier;
                }
                else ESP_LOGW(TAG, "current multiplier from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_CURRENT_DIVISOR) {
                if (plug) {
                    plug->current_divisor = event.data.current_divisor;
                }
                else ESP_LOGW(TAG, "current divisor from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_SUMMATION_MULTIPLIER) {
                if (plug) {
                    plug->summation_multiplier = event.data.summation_multiplier;
                }
                else ESP_LOGW(TAG, "summation multiplier from unkown smart plug");
            }
            else if (event.type == ZIGBEE_EVENT_SUMMATION_DIVISOR) {
                if (plug) {
                    plug->summation_divisor = event.data.summation_divisor;
                }
                else ESP_LOGW(TAG, "summation divisor form unkown smart plug");
            }
            else ESP_LOGW(TAG, "unknown event type");
        }
    }
}

// public methods. Will be used through controller interface eventually. 

void ZigbeeCoordinator::get_energy_consumption(uint16_t short_addr){
    for (const auto& [key, value] : devices) {
        esp_zigbee_lock_acquire(portMAX_DELAY);
        read_electrical_measurement_values(key, value.endpoint); 
        esp_zigbee_lock_release();
    }
}

void ZigbeeCoordinator::get_electrical_values(uint16_t short_addr){
    for (const auto& [key, value] : devices) {
        esp_zigbee_lock_acquire(portMAX_DELAY);
        read_energy_consumption_value(key, value.endpoint); 
        esp_zigbee_lock_release();
    }
}

int ZigbeeCoordinator::check_device_count(){
    for (const auto& [key, value] : devices) {
        printf("Device: 0x%04hx\n", key); 
    }
    return devices.size(); 
}

void ZigbeeCoordinator::toggle_smart_plug(uint16_t short_addr){
    auto &plug = devices[short_addr];
    if (devices.contains(short_addr)) {
        esp_zigbee_lock_acquire(portMAX_DELAY);
        send_toggle_smart_plug(short_addr, plug.endpoint);
        esp_zigbee_lock_release(); 
    }
    else ESP_LOGW(TAG, "Trying to toggle unkown smart plug");
}

// private methods 


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
ezb_err_t ZigbeeCoordinator::zdo_find_smart_plug_device(uint16_t dst_addr){
    //smartPlug.short_address = dst_addr;

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
                //smartPlug.endpoint = result->rsp->match_list[i];
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
    //ESP_LOGW(TAG, "plug short address: %d", dst_short_addr);

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

    //ESP_RETURN_ON_ERROR(ezb_address_extended_by_short(dst_short_addr, &bind_req.field.dst_addr.extended_addr), TAG,
                        //"Failed to get extended address for destination device(0x%04hx)", dst_short_addr);
    //ret = ezb_zdo_bind_req(&bind_req);
    //bind_req.field.cluster_id = EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT;
    //ret = ezb_zdo_bind_req(&bind_req)

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
            //ESP_LOGW(TAG, "SHORT ADDRESS: %d", (uint16_t) user_ctx);
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
}