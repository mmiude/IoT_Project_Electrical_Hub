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

    ESP_LOGI(TAG, "Starting the coordinatortask");
    if (event_queue_t == NULL) ESP_LOGE(TAG, "QUEUE NOT INITIALIZED!");
    ESP_LOGW(TAG, "run() event_queue = %p", event_queue_t);
    zigbee_event event; 

    while (true) {
        if (xQueueReceive(event_queue_t, &event, portMAX_DELAY) == pdPASS) {
            if (event.type == ZIGBEE_EVENT_DEVICE_JOINED) {
                if (devices.contains(event.short_address)){
                    ESP_LOGI(TAG, "smart plug rejoined: 0x%04hx", event.short_address);
                }
                else {
                    ESP_LOGI(TAG, "new smart plug joined: 0x%04hx", event.short_address);
                    binding_short_addr = event.short_address; 
                    devices[event.short_address].short_addr = event.short_address;
                    esp_zigbee_lock_acquire(portMAX_DELAY);
                    zdo_find_smart_plug_device(event.short_address);
                    esp_zigbee_lock_release();
                }
            } 
            else if (event.type == ZIGBEE_EVENT_DEVICE_LEFT) {
                ESP_LOGI(TAG, "smart plug left: 0x%04hx", event.short_address);
                if (devices.contains(event.short_address)) {
                    devices.erase(event.short_address); 
                    ESP_LOGI(TAG, "smart plug deleted from devices.");
                }
                else ESP_LOGW(TAG, "unkonwn devices left");
            }
            else if (event.type == ZIGBEE_EVENT_ONOFF_REPORT) {
                ESP_LOGI(TAG, "smart plug: 0x%04hx on/off report (ON/OFF state: %s)", event.short_address, event.data.is_on ? "ON" : "OFF");
            }
            else if (event.type == ZIGBEE_EVENT_POWER_REPORT) {
                ESP_LOGI(TAG, "smart plug: 0x%04hx power report", event.short_address);
            }
            else if (event.type == ZIGBEE_EVENT_VOLTAGE_REPORT) {
                ESP_LOGI(TAG, "smart plug: 0x%04hx voltage report", event.short_address);
            }
            else if (event.type == ZIGBEE_EVENT_CURRENT_REPORT) {
                ESP_LOGI(TAG, "smart plug: 0x%04hx current report", event.short_address);
            }
            else if (event.type == ZIGBEE_EVENT_SUMMATION_REPORT) {
                ESP_LOGI(TAG, "smart plug: 0x%04hx summation report", event.short_address);
            }
            else ESP_LOGW(TAG, "unknown event type");
        }
    }
}

// public methods. Will be used through controller interface eventually. 

void ZigbeeCoordinator::energy_consumption(){
    for (const auto& [key, value] : devices) {
        esp_zigbee_lock_acquire(portMAX_DELAY);
        read_electrical_measurement_values(key, value.endpoint); 
        esp_zigbee_lock_release();
    }
}

void ZigbeeCoordinator::electrical_values(){
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
            //read_electrical_measurement_multipliers(smartPlug.short_address, smartPlug.endpoint);
            //read_energy_consumption_multipliers(smartPlug.short_address, smartPlug.endpoint);
            esp_zigbee_lock_release(); 

        } else {
            ESP_LOGE(TAG, "Failed to bind smart plug device with status (0x%02x)", result->rsp->status);
        }
    } else {
        ESP_LOGE(TAG, "Failed to bind smart plug device with error (0x%04x)", result->error);
    }
}