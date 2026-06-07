/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

/*#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "protocol_examples_common.h"
#include "esp_wifi.h"
// #endif*/


/*#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE
#include "esp_coexist.h"
#endif*/

//#include "light_driver.h"
#include "alarm_timer.h"

#include "esp_zigbee.h"

#include "ezbee/zha.h"
#include "zigbee_gateway.h"

static const char *TAG = "ZIGBEE_COORDINATOR";

typedef struct {
    uint16_t short_address;
    uint8_t endpoint;
    bool online;
    bool is_on; 

    uint16_t current_divisor;
    uint16_t current_multiplier;
    uint16_t voltage_divisor;
    uint16_t voltage_multiplier;
    uint16_t power_divisor;
    uint16_t power_multiplier;
    bool support_metering;
    uint32_t metering_divisor;
    uint32_t metering_multiplier;

    float active_power;
    float voltage; 
    float current;
    float summation_kwh; 

} smartplugInfo;

static smartplugInfo smartPlug;

esp_err_t send_configure_reporting(uint16_t dst_addr, uint8_t dst_ep);

/*static esp_err_t deferred_driver_init(void)
{
    static bool is_inited = false;

    ESP_RETURN_ON_FALSE(!is_inited, ESP_OK, TAG, "Deferred driver already initialized");

    light_driver_init(false);
    is_inited = true;

    return ESP_OK;
}*/

/*static ezb_err_t read_available_attributes(uint16_t dst_addr, uint8_t dst_ep)
{

    uint16_t attr_fields[3] = {
            EZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID,  // 0x0000 - cumulative kWh
            EZB_ZCL_ATTR_METERING_MULTIPLIER_ID,                   // 0x0301 - to convert raw value
            EZB_ZCL_ATTR_METERING_DIVISOR_ID, 
    };

    const ezb_zcl_read_attr_cmd_t disc_attr_cmd = {
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
            .attr_number = 3,
            .attr_field = attr_fields, 
        },
    };

    ezb_err_t err = ezb_zcl_read_attr_cmd_req(&disc_attr_cmd);
    return err == EZB_ERR_NONE ? ESP_OK : ESP_FAIL; 
}*/

static ezb_err_t read_electrical_measurement_multipliers(uint16_t dst_addr, uint8_t dst_ep)
{
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

static ezb_err_t read_electrical_measurement_values(uint16_t dst_addr, uint8_t dst_ep)
{
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

static ezb_err_t read_energy_consumption_multipliers(uint16_t dst_addr, uint8_t dst_ep)
{
    uint16_t metering_attrs[] = {
        EZB_ZCL_ATTR_METERING_MULTIPLIER_ID,                   // 0x0301 - to convert raw value
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

static ezb_err_t read_energy_consumption_value(uint16_t dst_addr, uint8_t dst_ep)
{
    uint16_t metering_attrs[] = {
        EZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID,  
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
            .attr_number = 1,
            .attr_field = metering_attrs,
        },
    };

    ezb_err_t err = ezb_zcl_read_attr_cmd_req(&read_attr_cmd);
    return err == EZB_ERR_NONE ? ESP_OK : ESP_FAIL; 
}

static esp_err_t read_plug_on_off_state(uint16_t dst_addr, uint8_t dst_ep)
{
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

static esp_err_t send_toggle_smart_plug(uint16_t dst_addr, uint8_t dst_ep)
{
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

static void zdo_bind_smart_plug_result(const ezb_zdp_bind_req_result_t *result, void *user_ctx)
{
    assert(result);
    if (result->error == EZB_ERR_NONE) {
        if (result->rsp && result->rsp->status == EZB_ZDP_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Bound smart plug device successfully");
            //ESP_LOGW(TAG, "SHORT ADDRESS: %d", (uint16_t) user_ctx);
            //send_configure_reporting(smart_plugs[smart_index], 1);
            esp_zigbee_lock_acquire(portMAX_DELAY);
            send_configure_reporting(smartPlug.short_address, smartPlug.endpoint);
            read_electrical_measurement_multipliers(smartPlug.short_address, smartPlug.endpoint);
            read_energy_consumption_multipliers(smartPlug.short_address, smartPlug.endpoint);
            esp_zigbee_lock_release(); 

        } else {
            ESP_LOGE(TAG, "Failed to bind smart plug device with status (0x%02x)", result->rsp->status);
        }
    } else {
        ESP_LOGE(TAG, "Failed to bind smart plug device with error (0x%04x)", result->error);
    }
}

static ezb_err_t zdo_bind_smart_plug_device(uint16_t dst_short_addr, uint8_t dst_ep)
{
    ezb_err_t          ret      = EZB_ERR_FAIL;
    ESP_LOGW(TAG, "plug short address: %d", dst_short_addr);

    ezb_zdo_bind_req_t bind_req = {
        .dst_nwk_addr = dst_short_addr,
        .field =
            {
                .src_ep        = dst_ep, 
                .dst_addr_mode = EZB_ADDR_MODE_EXT,
                .dst_ep        = ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID,
                .cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF,
            },
        .cb       = zdo_bind_smart_plug_result,
        //.user_ctx = (void *) dst_short_addr, //(void*) dst_short_addr,
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

static void zdo_find_smart_plug_device_result(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx)
{
    assert(result);
    if (result->error == EZB_ERR_NONE) {
        if (result->rsp && result->rsp->status == EZB_ZDP_STATUS_SUCCESS && result->rsp->match_length > 0 &&
            result->rsp->match_list) {
            for (size_t i = 0; i < result->rsp->match_length; i++) {
                ESP_LOGW(TAG, "possible id: %d", result->rsp->match_list[i]);
                smartPlug.endpoint = result->rsp->match_list[i];
                zdo_bind_smart_plug_device(result->rsp->nwk_addr_of_interest, result->rsp->match_list[i]);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to find smart plug device in the network with error(0x%04x)", result->error);
    }
}

static ezb_err_t zdo_find_smart_plug_device(uint16_t dst_addr)
{   
    /*if (smart_index > 2) smart_index = 0;
    smart_plugs[smart_index] = dst_addr;
    smart_index++; */

    smartPlug.short_address = dst_addr;

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
        .user_ctx = NULL,
    };
    ret = ezb_zdo_match_desc_req(&req);
    if (ret == EZB_ERR_NONE) {
        ESP_LOGI(TAG, "Attempt to find smart_plug device");
    } else {
        ESP_LOGE(TAG, "Failed to find smart_plug device with error(0x%04x)", ret);
    }
    return ret;
} 

static void esp_zigbee_alarm_bdb_commissioning(alarm_timer_arg_t arg)
{
    //mandatory to acquire the lock before calling any Zigbee SDK APIs
    esp_zigbee_lock_acquire(portMAX_DELAY);
    (void)ezb_bdb_start_top_level_commissioning(arg);
    esp_zigbee_lock_release();
}

//think of this like a interrupt handler for zigbee signals
static bool esp_zigbee_app_signal_handler(const ezb_app_signal_t *app_signal)
{   
    //Obtains the type of the application signal 
    ezb_app_signal_type_t signal_type = ezb_app_signal_get_type(app_signal);

    switch (signal_type) {
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION); //start top level comissioning procedure with specified mode mask
        break;
    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case EZB_BDB_SIGNAL_DEVICE_REBOOT: {
        ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal)); //obtains pointer to parameters passed with application signal 
        if (status == EZB_BDB_STATUS_SUCCESS) {
            //ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", ezb_bdb_is_factory_new() ? "" : " non");
            if (ezb_bdb_is_factory_new()) {
                ESP_ERROR_CHECK(ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_FORMATION));
            } else {
                ezb_bdb_open_network(180); // open zigbee network (takes permit duration as parameter)
                ESP_LOGI(TAG, "Device reboot");
            }
        } else {
            ESP_LOGW(TAG, "The %s failed with status(0x%02x), please retry", ezb_app_signal_to_string(signal_type), status);
            alarm_timer_schedule(esp_zigbee_alarm_bdb_commissioning, EZB_BDB_MODE_INITIALIZATION, 1000); // if network formation fails this will try again
        }
    } break;
    case EZB_BDB_SIGNAL_FORMATION: {
        ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal));
        if (status == EZB_BDB_STATUS_SUCCESS) {
            ezb_extpanid_t extended_pan_id;
            ezb_nwk_get_extended_panid(&extended_pan_id); // get extended network id
            ESP_LOGI(TAG, "Formed network successfully: PAN ID(0x%04hx, EXT: 0x%llx), Channel(%d), Short Address(0x%04hx)",
                     ezb_nwk_get_panid(), extended_pan_id.u64, ezb_nwk_get_current_channel(), ezb_nwk_get_short_address());
            ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING); // stat steering (devices can find network and join)
        } else {
            ESP_LOGW(TAG, "Failed to form network with status(0x%02x)", status);
            alarm_timer_schedule(esp_zigbee_alarm_bdb_commissioning, EZB_BDB_MODE_NETWORK_FORMATION, 1000); // try again
        }
    } break;
    case EZB_BDB_SIGNAL_STEERING: {
        ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal));
        if (status == EZB_BDB_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Network steering completed");
        } else {
            ESP_LOGW(TAG, "Failed to steering network with status(0x%02x)", status);
            alarm_timer_schedule(esp_zigbee_alarm_bdb_commissioning, EZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
    } break;
    case EZB_ZDO_SIGNAL_DEVICE_ANNCE: {
        const ezb_zdo_signal_device_annce_params_t *dev_annce_params = ezb_app_signal_get_params(app_signal);
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->short_addr); // devices shrot address 
        zdo_find_smart_plug_device(dev_annce_params->short_addr);
        //smart_plugs = dev_annce_params->short_addr;
        //send_configure_reporting(dev_annce_params->short_addr, 1);
    } break;
    case EZB_ZDO_SIGNAL_LEAVE_INDICATION: {
        const ezb_zdo_signal_leave_indication_params_t *leave_ind_params = ezb_app_signal_get_params(app_signal);
        ESP_LOGI(TAG, "Zigbee Node(0x%04hx) is leaving network", leave_ind_params->short_addr); // leaving devices information in leave_ind_params
    } break;
    case EZB_NWK_SIGNAL_PERMIT_JOIN_STATUS: {
        uint8_t duration = *(uint8_t *)ezb_app_signal_get_params(app_signal);
        if (duration) {
            ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", ezb_nwk_get_panid(), duration); // network ezb_nwk_get_panid() gives network id 
        } else {
            ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", ezb_nwk_get_panid());
        }
    } break;
    default:
        ESP_LOGI(TAG, "Zigbee APP Signal: %s(type: 0x%02x)", ezb_app_signal_to_string(signal_type), signal_type); // default if some signal not specified comes in
        break;
    }
    return true;
}

/*
static void zcl_core_set_attr_value_handler(ezb_zcl_set_attr_value_message_t *message) 

// we probably need to add here to handle the energy consumption requests and any other we need (command handling)
// EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT - if we can set so that measurement is sent if value changes or do we poll every 15 mins? ezb_err_t ezb_zcl_config_report_cmd_req(const ezb_zcl_config_report_cmd_t *cmd_req)
// SEND ON/OFF COMMANDS - if device needs to be set off
// ASK ON/OFF STATUS - can end device send this if changed? 
{
    ESP_RETURN_ON_FALSE(message, , TAG, "message is empty");
    ESP_LOGI(TAG, "ZCL SetAttributeValue message for endpoint(%d) cluster(0x%04x) %s with status(0x%02x)", message->info.dst_ep,
             message->info.cluster_id, message->info.cluster_role == EZB_ZCL_CLUSTER_SERVER ? "server" : "client",
             message->info.status);
    if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF) {
        light_driver_set_power(*(uint8_t *)message->in.attribute.data.value);
        ESP_LOGI(TAG, "Set On/Off: %d", *(uint8_t *)message->in.attribute.data.value);
    } else {
        ESP_LOGW(TAG, "Unsupported cluster ID(0x%04x)", message->info.cluster_id);
    }
}
    

static void esp_zigbee_zcl_core_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message)
{
    switch (callback_id) {
    case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:
        zcl_core_set_attr_value_handler(message);
        break;
    case EZB_ZCL_CORE_DEFAULT_RSP_CB_ID: {
        ezb_zcl_cmd_default_rsp_message_t *default_rsp = (ezb_zcl_cmd_default_rsp_message_t *)message;
        ESP_LOGI(TAG, "Received ZCL Default Response with status(0x%02x)", default_rsp->in.status_code);
    } break;
    default:
        ESP_LOGW(TAG, "ZCL Core Action: ID(0x%04lx)", callback_id);
        break;
    }
}*/

static void zcl_core_report_attr_handler(ezb_zcl_cmd_report_attr_message_t *message)
{
    ESP_RETURN_ON_FALSE(message, , TAG, "message is empty");

    ezb_zcl_report_attr_variable_t *response = message->in.variables; 
    const ezb_zcl_cmd_hdr_t *header = message->in.header; 

    ESP_LOGI(TAG, "Attr report from smart plug(%d) addr(%d) attr(0x%04x) data type(0x%04x)",
        header->src_ep,
        header->src_addr,
        response->attr_id,
        response->attr_type);

    if (response->attr_id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) 
    {
        bool is_on = *(bool *)response->attr_value;
        smartPlug.is_on = is_on; 
        ESP_LOGI(TAG, "Plug ep(%d) on/off state: %s", header->src_addr.u.short_addr, is_on ? "ON" : "OFF");
    }
    else
    {
        ESP_LOGW(TAG, "Unhandled report cluster (0x04x)", message->info.cluster_id);
    }
}

static void zcl_core_read_attrbute_response(ezb_zcl_cmd_read_attr_rsp_message_t *message)
{
    ezb_zcl_read_attr_rsp_variable_t *response = message->in.variables;
    const ezb_zcl_cmd_hdr_t *header = message->in.header;

    ESP_LOGW(TAG, "SMART PLUG ATTRIBUTE RESPONSE: ep(%d), short address(%d)", header->src_ep, header->src_addr.u.short_addr);
    // TODO: here we need to somehow set metering -> false if not supported and if electrical is not supported we don't add that plug at all. 
    if (response->status != 0) ESP_LOGE(TAG, "RESPONSE STATUS ERROR: attr(0x%04x) smart plug(%d)", response->attr_id, header->src_addr.u.short_addr);

    while (response != NULL && response->status == 0) {
        
        switch (message->info.cluster_id) 
        {
        case EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT: // value is in response->attr_value
            
            switch (response->attr_id)
            {
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID:
                    int16_t power = *(int16_t *) response->attr_value;
                    //ESP_LOGW(TAG, "Electrical active power: status: %d, type: %d, value: %d", response->status, response->attr_type, power);
                    smartPlug.active_power = power / (float)smartPlug.power_divisor * smartPlug.power_multiplier;
                    break;
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID:
                    uint16_t voltage = *(uint16_t *) response->attr_value; 
                    //ESP_LOGW(TAG, "Electrical rms voltage: status: %d, type: %d, value: %d", response->status, response->attr_type, voltage);
                    smartPlug.voltage = voltage / (float)smartPlug.voltage_divisor * smartPlug.voltage_multiplier;
                    break;
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID:
                    uint16_t current = *(uint16_t *) response->attr_value; 
                    //ESP_LOGW(TAG, "Electrical rms current: status: %d, type: %d, value: %d, divisor: %d", response->status, response->attr_type, current, smartPlug.current_divisor);
                    smartPlug.current = current / (float)smartPlug.current_divisor * smartPlug.current_multiplier;
                    break;
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_MULTIPLIER_ID:
                    uint16_t power_multi = *(uint16_t *) response->attr_value; 
                    //ESP_LOGW(TAG, "Electrical ac power multiplier: status: %d, type: %d, value: %d", response->status, response->attr_type, power_multi);
                    smartPlug.power_multiplier = power_multi;
                    break;
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_DIVISOR_ID:
                    uint16_t power_divi = *(uint16_t *) response->attr_value; 
                    //ESP_LOGW(TAG, "Electrical ac power divisor: status: %d, type: %d, value: %d", response->status, response->attr_type, power_divi);
                    smartPlug.power_divisor = power_divi;
                    break;
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_DIVISOR_ID:
                    uint16_t voltage_divi = *(uint16_t *) response->attr_value; 
                    //ESP_LOGW(TAG, "Electrical ac voltage divisor: status: %d, type: %d, value: %d", response->status, response->attr_type, voltage_divi);
                    smartPlug.voltage_divisor = voltage_divi;
                    break;
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_MULTIPLIER_ID:
                    uint16_t voltage_multi = *(uint16_t *) response->attr_value; 
                    //ESP_LOGW(TAG, "Electrical ac voltage multiplier: status: %d, type: %d, value: %d", response->status, response->attr_type, voltage_multi);
                    smartPlug.voltage_multiplier = voltage_multi; 
                    break;
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_MULTIPLIER_ID:
                    uint16_t current_multi = *(uint16_t *) response->attr_value; 
                    //ESP_LOGW(TAG, "Electrical ac current multiplier: status: %d, type: %d, value: %d", response->status, response->attr_type, current_multi);
                    smartPlug.current_multiplier = current_multi;
                    break;
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_DIVISOR_ID:
                    uint16_t current_divi = *(uint16_t *) response->attr_value; 
                    //ESP_LOGW(TAG, "Electrical ac current divisor: status: %d, type: %d, value: %d", response->status, response->attr_type, current_divi);
                    smartPlug.current_divisor = current_divi;
                    break;
                default:
                    //ESP_LOGE(TAG, "Unknown attribute id in attribute read response handler (electrical).");
                    break;
            }
            break;
        case EZB_ZCL_CLUSTER_ID_METERING: // value is in response->attr_value
            smartPlug.support_metering = true;

            switch (response->attr_id)
            {
                case EZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID:
                    uint64_t summation = *(uint64_t *) response->attr_value;
                    //ESP_LOGW(TAG, "Metering current summation: status: %d, type: %d, value: %d", response->status, response->attr_type, summation);
                    smartPlug.summation_kwh = summation / (float)smartPlug.metering_divisor * smartPlug.metering_multiplier;
                    break;
                case EZB_ZCL_ATTR_METERING_MULTIPLIER_ID:
                    uint32_t metering_multi = *(uint32_t *) response->attr_value;
                    //ESP_LOGW(TAG, "Metering multiplier: status: %d, type: %d, value: %d", response->status, response->attr_type, metering_multi);
                    smartPlug.metering_multiplier = metering_multi;
                    break;              
                case EZB_ZCL_ATTR_METERING_DIVISOR_ID:
                    uint32_t metering_divi = *(uint32_t *) response->attr_value;
                    //ESP_LOGW(TAG, "Metering divisor: status: %d, type: %d, value: %d", response->status, response->attr_type, metering_divi);
                    smartPlug.metering_divisor = metering_divi;
                    break;
                default:
                    ESP_LOGE(TAG, "Unknown attribute id in attribute read response handler (metering).");
                    break;
            }
            break;
        default:
            ESP_LOGE(TAG, "Unknown cluster id in attribute read response handler.");
            break;
        }
        response = response->next;
    }
}

static void esp_zigbee_zcl_core_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message) {

    ESP_LOGI(TAG, "ZCL action callback ID: 0x%04lx", callback_id);
    
    switch (callback_id) {

        case EZB_ZCL_CORE_REPORT_ATTR_CB_ID: //EZB_ZCL_CORE_REPORT_ATTR_CB_ID 
            zcl_core_report_attr_handler((ezb_zcl_cmd_report_attr_message_t *)message);
            break;

        case EZB_ZCL_CORE_READ_ATTR_RSP_CB_ID:
            zcl_core_read_attrbute_response((ezb_zcl_cmd_read_attr_rsp_message_t *)message);
            break;

        case EZB_ZCL_CORE_CONFIG_REPORT_RSP_CB_ID: // 0x0003
            ezb_zcl_cmd_config_report_rsp_message_t *config_response = (ezb_zcl_cmd_config_report_rsp_message_t *)message;
            const ezb_zcl_cmd_hdr_t *config_header = config_response->in.header;

            ESP_LOGI(TAG, "Configure reporting response from cluster(0x%04x) smart plug(%d)", config_response->info.cluster_id, config_header->src_addr.u.short_addr);

            ezb_zcl_config_report_rsp_variable_t *config_response_variable = config_response->in.variables;
            while (config_response_variable != NULL) {
                if (config_response_variable->status == EZB_ZCL_STATUS_SUCCESS) {
                    ESP_LOGI(TAG, "  All attributes accepted (status SUCCESS)");
                } else {
                    ESP_LOGW(TAG, "  attr(0x%04x) FAILED with status(0x%02x)",
                     config_response_variable->attr_id, config_response_variable->status);
                }
                config_response_variable = config_response_variable->next;
            }
            config_response->out.result = EZB_ZCL_STATUS_SUCCESS;
            break;

        case EZB_ZCL_CORE_DEFAULT_RSP_CB_ID: // THIS WORKSSS YAYYYYY 
            // Fires after spikestriker sends ON/OFF commands - check if succeeded
            ezb_zcl_cmd_default_rsp_message_t *default_response = (ezb_zcl_cmd_default_rsp_message_t *)message;
            const ezb_zcl_cmd_hdr_t *default_header = default_response->in.header;
            ESP_LOGI(TAG, "Command response from ep(%d) smart plug(%d): status(0x%02x) %s",
                 default_response->info.dst_ep,
                 default_header->src_addr.u.short_addr,
                 default_response->info.status,
                 default_response->info.status == 0 ? "OK" : "FAILED");
            break;

        default:
            ESP_LOGW(TAG, "Unhandled ZCL callback ID(0x%04lx)", callback_id);
            break;
    }
}

/*static bool raw_frame_handler(const ezb_zcl_raw_frame_t *raw_frame) // to see raw frames in stack level
{
    ESP_LOGI(TAG, "Raw ZCL frame received:");
    //ESP_LOGI(TAG, "  frame_type: 0x%02x", raw_frame->header->frame_ctrl.frame_type);
    ESP_LOGI(TAG, "  cmd_id: 0x%02x", raw_frame->header->cmd_id);
    ESP_LOGI(TAG, "  payload_length: %d", raw_frame->payload_length);
    return false;  // false = let stack process it normally, don't drop it
} */

esp_err_t send_configure_reporting(uint16_t dst_addr, uint8_t dst_ep)
{
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
                .u.short_addr =  dst_addr
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

// here we need to check that should server be changed to client and see that all necessary clusters are added 
// also probably some signals need to be added to signal handler
esp_err_t esp_zigbee_create_zha_gateway_device(void)
{
    ezb_af_device_desc_t dev_desc = ezb_af_create_device_desc();
    ezb_zha_custom_gateway_config_t gateway_cfg = EZB_ZHA_CUSTOM_GATEWAY_CONFIG();
    ezb_af_ep_desc_t ep_desc = ezb_zha_create_custom_gateway(ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID, &gateway_cfg);
    ezb_zcl_cluster_desc_t basic_desc = {0};

    basic_desc = ezb_af_endpoint_get_cluster_desc(ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_CLIENT); // should these be client 
    ezb_zcl_basic_cluster_desc_add_attr(basic_desc, EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)ESP_MANUFACTURER_NAME);
    ezb_zcl_basic_cluster_desc_add_attr(basic_desc, EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)ESP_MODEL_IDENTIFIER);
    //ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_on_off_create_cluster_desc(NULL, EZB_ZCL_CLUSTER_CLIENT));
    ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_on_off_create_cluster_desc(NULL, EZB_ZCL_CLUSTER_CLIENT));
    // Creat and add the electrical medasurement client cluster
    ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_electrical_measurement_create_cluster_desc(NULL, EZB_ZCL_CLUSTER_CLIENT));

    // Creat and add the Metering Client Cluster
    ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_metering_create_cluster_desc(NULL, EZB_ZCL_CLUSTER_CLIENT));

    ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc));
    ESP_ERROR_CHECK(ezb_af_device_desc_register(dev_desc));

    ezb_zcl_core_action_handler_register(esp_zigbee_zcl_core_action_handler); 
    //ezb_zcl_raw_command_handler_register(raw_frame_handler);

    return ESP_OK;
}

esp_err_t esp_zigbee_setup_commissioning(void)
{
    ezb_aps_secur_enable_distributed_security(false);
    ESP_ERROR_CHECK(ezb_bdb_set_primary_channel_set(ESP_ZIGBEE_PRIMARY_CHANNEL_MASK));
    ESP_ERROR_CHECK(ezb_bdb_set_secondary_channel_set(ESP_ZIGBEE_SECONDARY_CHANNEL_MASK));
    ESP_ERROR_CHECK(ezb_app_signal_add_handler(esp_zigbee_app_signal_handler));

    return ESP_OK;
}

static void esp_zigbee_stack_main_task(void *pvParameters)
{
    esp_zigbee_config_t zigbee_config = ESP_ZIGBEE_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(esp_zigbee_init(&zigbee_config));

    ESP_ERROR_CHECK(esp_zigbee_setup_commissioning());

    ESP_ERROR_CHECK(esp_zigbee_create_zha_gateway_device());

    ESP_ERROR_CHECK(esp_zigbee_start(false));

    esp_zigbee_launch_mainloop();

    esp_zigbee_deinit();

    vTaskDelete(NULL);
}

static void dummy_toggle_task(void *pvParameters)
{   
    while (1) {

        esp_zigbee_lock_acquire(portMAX_DELAY);
        read_energy_consumption_value(smartPlug.short_address, smartPlug.endpoint);
        read_electrical_measurement_values(smartPlug.short_address, smartPlug.endpoint);
        esp_zigbee_lock_release(); 

        vTaskDelay(pdMS_TO_TICKS(500));

        printf("SMART PLUG: %d ****** INFO ***** \n", smartPlug.short_address);
        printf("plug on/off state: %s\n", smartPlug.is_on ? "ON" : "OFF");
        printf("current: %.4f A\n", smartPlug.current);
        printf("voltage: %.2f V\n", smartPlug.voltage);
        printf("active power: %.2f W\n", smartPlug.active_power);
        printf("energy summation: %.2f kwh\n", smartPlug.summation_kwh);
        vTaskDelay(pdMS_TO_TICKS(20000));
        
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME));

    ESP_LOGI(TAG, "Start ESP Zigbee Stack");
    xTaskCreate(esp_zigbee_stack_main_task, "Zigbee_main", 4096 * 2, NULL, 5, NULL);
    xTaskCreate(dummy_toggle_task, "Toggle test", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
}
