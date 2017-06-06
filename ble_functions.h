#ifndef ble_functions_h
#define ble_functions_h
#include <stdint.h>
#include <Arduino.h>

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

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void profile_KEYBLE_eventhandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
//static void gattc_profile_b_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

//#define KEYBLE_UUID_APPL_SRVC { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x23, 0x15 ,0x00,0x00} This is the nordic button service
/*
Define the UUID's to listen to
  -> Service is the main LEGO i/o service
  -> MOTOR_UUID is the characteristic for the output commands
*/
static esp_gatt_id_t lego_output_characteristic;
static esp_gatt_id_t lego_input_characteristic;
static int succes = 0;
#define WEDO_INPUT 0
#define WEDO_OUTPUT 1


#define KEYBLE_UUID_APPL_SRVC           { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x0e, 0x4f ,0x00,0x00} //4f0e is the main LEGO i/o stream
#define KEYBLE_UUID_APPL_SRVC_SEND_DESC { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x60, 0x15 ,0x00,0x00} //Sensor value characteristic
#define OUTPUT_UUID                     { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x65, 0x15 ,0x00,0x00} //1565 = OUTPUT UUID 'Output Command'
#define INPUT_UUID                      { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x63, 0x15 ,0x00,0x00} //1563 = INPUT UUID  'Input Command'


/*
This is to get the button status to check if the notification stream works (5-6-2017, it works!)
#define KEYBLE_UUID_APPL_SRVC           { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x23, 0x15 ,0x00,0x00} //4f0e is the main LEGO i/o stream
#define KEYBLE_UUID_APPL_SRVC_SEND_DESC { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x26, 0x15 ,0x00,0x00} //Sensor value characteristic
#define OUTPUT_UUID                     { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x28, 0x15 ,0x00,0x00} //1565 = OUTPUT UUID 'Output Command'
#define INPUT_UUID                      { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x29, 0x15 ,0x00,0x00} //1563 = INPUT UUID  'Input Command'
*/
//#define KEYBLE_UUID_APPL_SRVC {0x00,0x00,0x15 ,0x23, 0x12,0x12, 0xef, 0xde, 0x15,0x23, 0x78,0x5f, 0xea,0xbc, 0xd1,23}
//#define KEYBLE_UUID_APPL_SRVC_SEND_DESC {0x00,0x00,0x15 ,0x26, 0x12,0x12, 0xef, 0xde, 0x15,0x23, 0x78,0x5f, 0xea,0xbc, 0xd1,23}

static esp_gatt_srvc_id_t KEYBLE_application_service = {
    .id = {
        .uuid = {
            .len = ESP_UUID_LEN_128,
            .uuid = {.uuid128 = KEYBLE_UUID_APPL_SRVC,},
        },
        .inst_id = 0,
    },
    .is_primary = true,
};

static bool foundKEYBLE_application_service = true;

static esp_gatt_id_t scan_desr;



static esp_gatt_id_t notify_descr_id = {
    .uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = GATT_UUID_CHAR_CLIENT_CONFIG,},
    },
    .inst_id = 0,
};
#define BT_BD_ADDR_STR         "%02x:%02x:%02x:%02x:%02x:%02x"
#define BT_BD_ADDR_HEX(addr)   addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]

static bool connect = false;


static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30
};


#define PROFILE_NUM 1
#define PROFILE_KEYBLE_APP_ID 0
#define PROFILE_B_APP_ID 1

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
};

 //One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_KEYBLE_APP_ID] = {
        .gattc_cb = profile_KEYBLE_eventhandler,
        .gattc_if = ESP_GATT_IF_NONE,        /*Not get the gatt_if, so initial is ESP_GATT_IF_NONE*/
    },
    //[PROFILE_B_APP_ID] = {
      //  .gattc_cb = gattc_profile_b_event_handler,
        //.gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE*/
    //},
};
static void (*globalHandler)(uint8_t*,int);
void gattc_client_test(void);
static int connected = false;
void writeBLECommand(int type,uint8_t* command,int size);
void addBLEhandler(void (*f)(uint8_t*,int));
void setName(const char* name);
static int recieved;
int getBLEReady();
int getBLEConnected();
const static char* wedo_name;
#endif
