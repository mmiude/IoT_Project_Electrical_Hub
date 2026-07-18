/* Hardware abstraction implemented with C. 
Handles Zigbee network, stack and raw signals.*/

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "alarm_timer.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "zigbee_gateway.h"

static const char *TAG = "ZIGBEE_GATWAY";

static QueueHandle_t event_queue = NULL;

QueueHandle_t zigbee_gateway_get_queue()
{
    if (event_queue == NULL) {
        event_queue = xQueueCreate(20, sizeof(zigbee_event));
    }
    ESP_LOGI(TAG, "Gateway queue created: %p", event_queue); 
    return event_queue;
}

 
static void esp_zigbee_alarm_bdb_commissioning(alarm_timer_arg_t arg) 
{
    //mandatory to acquire the lock before calling any Zigbee SDK APIs
    esp_zigbee_lock_acquire(portMAX_DELAY);
    (void)ezb_bdb_start_top_level_commissioning(arg);
    esp_zigbee_lock_release();
}

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
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->short_addr);
        zigbee_event event = {.type = ZIGBEE_EVENT_DEVICE_JOINED, .short_address = dev_annce_params->short_addr};   
        ESP_LOGW(TAG, "Sending to queue handle: %p", event_queue);
        xQueueSend(event_queue, &event, 0); 
        //zdo_find_smart_plug_device(dev_annce_params->short_addr); binding will happen on coordinator side
        //smart_plugs = dev_annce_params->short_addr; // TODO: send short address to coordinator -> binds and sends configuration
        //send_configure_reporting(dev_annce_params->short_addr, 1);
    } break;
    case EZB_ZDO_SIGNAL_LEAVE_INDICATION: { 
        const ezb_zdo_signal_leave_indication_params_t *leave_ind_params = ezb_app_signal_get_params(app_signal);
        ESP_LOGI(TAG, "Zigbee Node(0x%04hx) is leaving network", leave_ind_params->short_addr); 
        zigbee_event event = {.type = ZIGBEE_EVENT_DEVICE_LEFT, .short_address = leave_ind_params->short_addr};   
        ESP_LOGW(TAG, "Sending to queue handle: %p", event_queue);
        xQueueSend(event_queue, &event, 0); 
    } break;
    case EZB_NWK_SIGNAL_PERMIT_JOIN_STATUS: { // TODO: how can we open network once requested? 
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
        ESP_LOGI(TAG, "Plug ep(%d) on/off state: %s", header->src_addr.u.short_addr, is_on ? "ON" : "OFF");
        zigbee_event event = {.type = ZIGBEE_EVENT_ONOFF_REPORT, .short_address = header->src_addr.u.short_addr, .data.is_on = is_on ? true : false};   
        ESP_LOGW("GATEWAY", "Sending to queue handle: %p", event_queue);
        xQueueSend(event_queue, &event, 0);
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
    if (response->status != 0) {
        ESP_LOGE(TAG, "RESPONSE STATUS ERROR: attr(0x%04x) smart plug(%d)", response->attr_id, header->src_addr.u.short_addr);
        zigbee_event event = {.type = ZIGBEE_EVENT_ATTRIBUTE_SUPPORT_ERROR, .short_address = header->src_addr.u.short_addr, .data.unsupported_attr = response->attr_id};
        xQueueSend(event_queue, &event, 0);
    } 

    while (response != NULL && response->status == 0) {
        
        switch (message->info.cluster_id) 
        {
        case EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT: // value is in response->attr_value
            
            switch (response->attr_id)
            {
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID: {
                    int16_t power = *(int16_t *) response->attr_value;
                    ESP_LOGW(TAG, "Electrical active power: status: %d, type: %d, value: %d", response->status, response->attr_type, power);
                    zigbee_event event = {.type = ZIGBEE_EVENT_POWER_REPORT, .short_address = header->src_addr.u.short_addr, .data.raw_power = power};   
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID: {
                    uint16_t voltage = *(uint16_t *) response->attr_value; 
                    ESP_LOGW(TAG, "Electrical rms voltage: status: %d, type: %d, value: %d", response->status, response->attr_type, voltage);
                    zigbee_event event = {.type = ZIGBEE_EVENT_VOLTAGE_REPORT, .short_address = header->src_addr.u.short_addr, .data.raw_voltage = voltage};   
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID: {
                    uint16_t current = *(uint16_t *) response->attr_value; 
                    ESP_LOGW(TAG, "Electrical rms current: status: %d, type: %d, value: %d", response->status, response->attr_type, current);
                    zigbee_event event = {.type = ZIGBEE_EVENT_CURRENT_REPORT, .short_address = header->src_addr.u.short_addr, .data.raw_current = current};   
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_MULTIPLIER_ID: {
                    uint16_t power_multi = *(uint16_t *) response->attr_value; 
                    ESP_LOGW(TAG, "Electrical ac power multiplier: status: %d, type: %d, value: %d", response->status, response->attr_type, power_multi);
                    zigbee_event event = {.type = ZIGBEE_EVENT_POWER_MULTIPLIER, .short_address = header->src_addr.u.short_addr, .data.raw_power = power_multi};   
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_DIVISOR_ID: {
                    uint16_t power_divi = *(uint16_t *) response->attr_value; 
                    ESP_LOGW(TAG, "Electrical ac power divisor: status: %d, type: %d, value: %d", response->status, response->attr_type, power_divi);
                    zigbee_event event = {.type = ZIGBEE_EVENT_POWER_DIVISOR, .short_address = header->src_addr.u.short_addr, .data.raw_power = power_divi};   
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_DIVISOR_ID: {
                    uint16_t voltage_divi = *(uint16_t *) response->attr_value; 
                    ESP_LOGW(TAG, "Electrical ac voltage divisor: status: %d, type: %d, value: %d", response->status, response->attr_type, voltage_divi);
                    zigbee_event event = {.type = ZIGBEE_EVENT_VOLTAGE_DIVISOR, .short_address = header->src_addr.u.short_addr, .data.raw_voltage = voltage_divi};   
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_MULTIPLIER_ID: {
                    uint16_t voltage_multi = *(uint16_t *) response->attr_value; 
                    ESP_LOGW(TAG, "Electrical ac voltage multiplier: status: %d, type: %d, value: %d", response->status, response->attr_type, voltage_multi);
                    zigbee_event event = {.type = ZIGBEE_EVENT_VOLTAGE_MULTIPLIER, .short_address = header->src_addr.u.short_addr, .data.raw_voltage = voltage_multi};   
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_MULTIPLIER_ID: {
                    uint16_t current_multi = *(uint16_t *) response->attr_value; 
                    ESP_LOGW(TAG, "Electrical ac current multiplier: status: %d, type: %d, value: %d", response->status, response->attr_type, current_multi);
                    zigbee_event event = {.type = ZIGBEE_EVENT_CURRENT_MULTIPLIER, .short_address = header->src_addr.u.short_addr, .data.raw_current = current_multi};   
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                case EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_DIVISOR_ID: {
                    uint16_t current_divi = *(uint16_t *) response->attr_value; 
                    ESP_LOGW(TAG, "Electrical ac current divisor: status: %d, type: %d, value: %d", response->status, response->attr_type, current_divi);
                    zigbee_event event = {.type = ZIGBEE_EVENT_CURRENT_DIVISOR, .short_address = header->src_addr.u.short_addr, .data.raw_current = current_divi};   
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                default:
                    ESP_LOGE(TAG, "Unknown attribute id in attribute read response handler (electrical).");
                    break;
            }
            break;
        case EZB_ZCL_CLUSTER_ID_METERING: // value is in response->attr_value
            //smartPlug.support_metering = true;

            switch (response->attr_id)
            {
                case EZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID: {
                    uint64_t summation = *(uint64_t *) response->attr_value;
                    ESP_LOGW(TAG, "Metering current summation: status: %d, type: %d, value: %d", response->status, response->attr_type, summation);
                    zigbee_event event = {.type = ZIGBEE_EVENT_SUMMATION_REPORT, .short_address = header->src_addr.u.short_addr, .data.raw_summation = summation};   
                    ESP_LOGW("GATEWAY", "Sending to queue handle: %p", event_queue);
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
                case EZB_ZCL_ATTR_METERING_MULTIPLIER_ID: {
                    uint32_t metering_multi = *(uint32_t *) response->attr_value;
                    ESP_LOGW(TAG, "Metering multiplier: status: %d, type: %d, value: %d", response->status, response->attr_type, metering_multi);
                    zigbee_event event = {.type = ZIGBEE_EVENT_SUMMATION_MULTIPLIER, .short_address = header->src_addr.u.short_addr, .data.raw_summation = metering_multi};   
                    ESP_LOGW("GATEWAY", "Sending to queue handle: %p", event_queue);
                    xQueueSend(event_queue, &event, 0);
                    break;   
                    }           
                case EZB_ZCL_ATTR_METERING_DIVISOR_ID: {
                    uint32_t metering_divi = *(uint32_t *) response->attr_value;
                    ESP_LOGW(TAG, "Metering divisor: status: %d, type: %d, value: %d", response->status, response->attr_type, metering_divi);
                    zigbee_event event = {.type = ZIGBEE_EVENT_SUMMATION_DIVISOR, .short_address = header->src_addr.u.short_addr, .data.raw_summation = metering_divi};   
                    ESP_LOGW("GATEWAY", "Sending to queue handle: %p", event_queue);
                    xQueueSend(event_queue, &event, 0);
                    break;
                    }
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

static void zcl_core_read_config_report_response(ezb_zcl_cmd_config_report_rsp_message_t *message)
{
    ezb_zcl_cmd_config_report_rsp_message_t *response = (ezb_zcl_cmd_config_report_rsp_message_t *)message;
    const ezb_zcl_cmd_hdr_t *header = response->in.header;

    ESP_LOGI(TAG, "Configure reporting response from cluster(0x%04x) smart plug(%d)", response->info.cluster_id, header->src_addr.u.short_addr);

    ezb_zcl_config_report_rsp_variable_t *response_variable = response->in.variables;
    while (response_variable != NULL) {
        if (response_variable->status == EZB_ZCL_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "  All attributes accepted (status SUCCESS)");
        } else {
            ESP_LOGW(TAG, "  attr(0x%04x) FAILED with status(0x%02x)",
            response_variable->attr_id, response_variable->status);
        }
        response_variable = response_variable->next;
    }
    response->out.result = EZB_ZCL_STATUS_SUCCESS;
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
            zcl_core_read_config_report_response((ezb_zcl_cmd_config_report_rsp_message_t *)message);
            break;
        case EZB_ZCL_CORE_DEFAULT_RSP_CB_ID:    
            ezb_zcl_cmd_default_rsp_message_t *response = (ezb_zcl_cmd_default_rsp_message_t *)message;
            const ezb_zcl_cmd_hdr_t *header = response->in.header;
            ESP_LOGI(TAG, "Command response from ep(%d) smart plug(%d): status(0x%02x) %s",
                 response->info.dst_ep,
                 header->src_addr.u.short_addr,
                 response->info.status,
                 response->info.status == 0 ? "OK" : "FAILED");
            break;
        default:
            ESP_LOGW(TAG, "Unhandled ZCL callback ID(0x%04lx)", callback_id);
            break;
    }
}

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

void esp_zigbee_stack_main_task(void *pvParameters) // coordinator task
{
    esp_zigbee_config_t zigbee_config = ESP_ZIGBEE_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(esp_zigbee_init(&zigbee_config));

    ESP_ERROR_CHECK(esp_zigbee_setup_commissioning());

    ESP_ERROR_CHECK(esp_zigbee_create_zha_gateway_device());

    ESP_ERROR_CHECK(esp_zigbee_start(false));

    ESP_LOGI(TAG, "Starting zigbee main task");
    esp_zigbee_launch_mainloop();

    esp_zigbee_deinit();

    vTaskDelete(NULL);
}


