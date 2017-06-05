/*
 * ble_help_func.c
 *
 *  Created on: Jan 23, 2017
 *      Author: root
 */

 #include <stdint.h>
 #include <string.h>
 #include <stdbool.h>
 #include <stdio.h>
 #include "controller.h"

 #include "bt.h"
 #include "bt_trace.h"
 #include "bt_types.h"
 #include "btm_api.h"
 #include "bta_api.h"
 #include "bta_gatt_api.h"
 #include "esp_gap_ble_api.h"
 #include "esp_gattc_api.h"
 #include "esp_gatt_defs.h"
 #include "esp_bt_main.h"

 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include <esp_log.h>
 #include <esp_bt_defs.h>
 #include "ble_help_functions.h"

const char* tag = "test"; // This is just the tag for debugging


void dump_adv_payload(uint8_t *payload) {
    uint8_t length;
    uint8_t ad_type;
    uint8_t sizeConsumed = 0;
    int finished = 0;
    int i;
    char text[31*2+1];
    //sprintf(text, "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x")
    while(!finished) {
        length = *payload;
        payload++;
        if (length != 0) {
            ad_type = *payload;
            payload += length;
            ESP_LOGD(tag, "Type: 0x%.2x, length: %d", ad_type, length);

        }

        sizeConsumed = 1 + length;
        if (sizeConsumed >=31 || length == 0) {
            finished = 1;
        }
    } // !finished
} // dump_adv_payload


const char *bt_dev_type_to_string(esp_bt_dev_type_t type) {
    switch(type) {
    case ESP_BT_DEVICE_TYPE_BREDR:
        return "ESP_BT_DEVICE_TYPE_BREDR";
    case ESP_BT_DEVICE_TYPE_BLE:
        return "ESP_BT_DEVICE_TYPE_BLE";
    case ESP_BT_DEVICE_TYPE_DUMO:
        return "ESP_BT_DEVICE_TYPE_DUMO";
    default:
        return "Unknown";
    }
} // bt_dev_type_to_string

const char *bt_addr_t_to_string(esp_ble_addr_type_t type) {
    switch(type) {
        case BLE_ADDR_TYPE_PUBLIC:
            return "BLE_ADDR_TYPE_PUBLIC";
        case BLE_ADDR_TYPE_RANDOM:
            return "BLE_ADDR_TYPE_RANDOM";
        case BLE_ADDR_TYPE_RPA_PUBLIC:
            return "BLE_ADDR_TYPE_RPA_PUBLIC";
        case BLE_ADDR_TYPE_RPA_RANDOM:
            return "BLE_ADDR_TYPE_RPA_RANDOM";
        default:
            return "Unknown addr_t";
    }
} // bt_addr_t_to_string

const char *bt_gap_search_event_type_to_string(uint32_t searchEvt) {
    switch(searchEvt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            return "ESP_GAP_SEARCH_INQ_RES_EVT";
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            return "ESP_GAP_SEARCH_INQ_CMPL_EVT";
        case ESP_GAP_SEARCH_DISC_RES_EVT:
            return "ESP_GAP_SEARCH_DISC_RES_EVT";
        case ESP_GAP_SEARCH_DISC_BLE_RES_EVT:
            return "ESP_GAP_SEARCH_DISC_BLE_RES_EVT";
        case ESP_GAP_SEARCH_DISC_CMPL_EVT:
            return "ESP_GAP_SEARCH_DISC_CMPL_EVT";
        case ESP_GAP_SEARCH_DI_DISC_CMPL_EVT:
            return "ESP_GAP_SEARCH_DI_DISC_CMPL_EVT";
        case ESP_GAP_SEARCH_SEARCH_CANCEL_CMPL_EVT:
            return "ESP_GAP_SEARCH_SEARCH_CANCEL_CMPL_EVT";
        default:
            return "Unknown event type";
    }
} // bt_gap_search_event_type_to_string

/*
 * Convert a BT GAP event type to a string representation.
 */
const char *bt_event_type_to_string(uint32_t eventType) {
    switch(eventType) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            return "ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT";
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            return "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT";
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            return "ESP_GAP_BLE_SCAN_RESULT_EVT";
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            return "ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT";
        default:
            return "Unknown event type";
    }
} // bt_event_type_to_string

int bt_compare_UUID128(uint8_t uuid1[ESP_UUID_LEN_128],
        uint8_t uuid2[ESP_UUID_LEN_128]) {
    for (int i = 0; i < ESP_UUID_LEN_128; i++) {
        if (uuid1[i] != uuid2[i]) {
            printf("%x != %x\n", uuid1[i], uuid2[i]);
            return 0;
        }
    }
    return 1;
}

int bt_compare_UUID128_reversed(uint8_t uuid1[ESP_UUID_LEN_128], uint8_t uuid2[ESP_UUID_LEN_128]) {
    for (int i = 0; i < ESP_UUID_LEN_128; i++) {
        if (uuid1[i] != uuid2[ESP_UUID_LEN_128-i-1]) {
            printf("%x != %x\n", uuid1[i], uuid2[ESP_UUID_LEN_128-i-1]);
            return 0;
        }
    }
    return 1;
}
