#include "ble_functions.h"

//esp_gatt_id_t lego_output_characteristic;


#define GATTC_TAG "ESPTEST"
static void profile_KEYBLE_eventhandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    uint16_t conn_id = 0;
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
    {
        ESP_LOGI(GATTC_TAG, "REG_EVT");
        break;
    }
    case ESP_GATTC_OPEN_EVT:
    {
        conn_id = p_data->open.conn_id;

        memcpy(gl_profile_tab[PROFILE_KEYBLE_APP_ID].remote_bda, p_data->open.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %d, mtu %d", conn_id, gattc_if, p_data->open.status, p_data->open.mtu);

        ESP_LOGI(GATTC_TAG, "REMOTE BDA  %02x:%02x:%02x:%02x:%02x:%02x",
                            gl_profile_tab[PROFILE_KEYBLE_APP_ID].remote_bda[0], gl_profile_tab[PROFILE_KEYBLE_APP_ID].remote_bda[1],
                            gl_profile_tab[PROFILE_KEYBLE_APP_ID].remote_bda[2], gl_profile_tab[PROFILE_KEYBLE_APP_ID].remote_bda[3],
                            gl_profile_tab[PROFILE_KEYBLE_APP_ID].remote_bda[4], gl_profile_tab[PROFILE_KEYBLE_APP_ID].remote_bda[5]
                         );

        // search for services
        esp_ble_gattc_search_service(gattc_if, conn_id, NULL);

        break;
        }
    case ESP_GATTC_SEARCH_RES_EVT: {
        esp_gatt_srvc_id_t *srvc_id = &p_data->search_res.srvc_id;
        conn_id = p_data->search_res.conn_id;
        ESP_LOGI(GATTC_TAG, "SEARCH RES: conn_id = %x", conn_id);
        if (srvc_id->id.uuid.len == ESP_UUID_LEN_16) {
            ESP_LOGI(GATTC_TAG, "UUID16: %x", srvc_id->id.uuid.uuid.uuid16);
        } else if (srvc_id->id.uuid.len == ESP_UUID_LEN_32) {
            ESP_LOGI(GATTC_TAG, "UUID32: %x", srvc_id->id.uuid.uuid.uuid32);
        } else if (srvc_id->id.uuid.len == ESP_UUID_LEN_128) {
            ESP_LOGI(GATTC_TAG, "UUID128: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x", srvc_id->id.uuid.uuid.uuid128[0],
                     srvc_id->id.uuid.uuid.uuid128[1], srvc_id->id.uuid.uuid.uuid128[2], srvc_id->id.uuid.uuid.uuid128[3],
                     srvc_id->id.uuid.uuid.uuid128[4], srvc_id->id.uuid.uuid.uuid128[5], srvc_id->id.uuid.uuid.uuid128[6],
                     srvc_id->id.uuid.uuid.uuid128[7], srvc_id->id.uuid.uuid.uuid128[8], srvc_id->id.uuid.uuid.uuid128[9],
                     srvc_id->id.uuid.uuid.uuid128[10], srvc_id->id.uuid.uuid.uuid128[11], srvc_id->id.uuid.uuid.uuid128[12],
                     srvc_id->id.uuid.uuid.uuid128[13], srvc_id->id.uuid.uuid.uuid128[14], srvc_id->id.uuid.uuid.uuid128[15]);

            // check if it is the KEYBLE application service

            if (!foundKEYBLE_application_service){
              foundKEYBLE_application_service = bt_compare_UUID128_reversed(srvc_id->id.uuid.uuid.uuid128, KEYBLE_application_service.id.uuid.uuid.uuid128);
            }

        } else {
            ESP_LOGE(GATTC_TAG, "UNKNOWN LEN %d\n", srvc_id->id.uuid.len);
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
    {
        conn_id = p_data->search_cmpl.conn_id;
        ESP_LOGI(GATTC_TAG, "SEARCH_CMPL: conn_id = %x, status %d", conn_id, p_data->search_cmpl.status);

        if (foundKEYBLE_application_service){
          printf("found KEYBLE application service\n");

          esp_err_t ret = ESP_OK;

          scan_desr.inst_id = 0;
          scan_desr.uuid.len = ESP_UUID_LEN_16;
          scan_desr.uuid.uuid.uuid16 = 0x1234;

          ret = esp_ble_gattc_get_characteristic(gattc_if, conn_id, &KEYBLE_application_service, NULL);
          printf("get char return: %d\n", ret);


        }
        else {
          printf("could not find KEYBLE application service\n");
        }



        break;
        }
    case ESP_GATTC_GET_CHAR_EVT:{
      printf("get char\n");
        if (p_data->get_char.status != ESP_GATT_OK) {
          printf("Get char error %x \n", p_data->get_char.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "GET CHAR: conn_id = %x, status %d", p_data->get_char.conn_id, p_data->get_char.status);
        ESP_LOGI(GATTC_TAG, "GET CHAR: srvc_id = %04x, char_id = %04x", p_data->get_char.srvc_id.id.uuid.uuid.uuid16, p_data->get_char.char_id.uuid.uuid.uuid16);

        uint8_t profile_uuid[ESP_UUID_LEN_128] = KEYBLE_UUID_APPL_SRVC_SEND_DESC;
        if ( bt_compare_UUID128(p_data->get_char.char_id.uuid.uuid.uuid128, profile_uuid)) {
          printf("found KEYBLE appserv send profile!\n");

          printf("register notify\n");
            esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[PROFILE_KEYBLE_APP_ID].remote_bda, &KEYBLE_application_service, &p_data->get_char.char_id);
        }
        uint8_t input_uuid[ESP_UUID_LEN_128] = INPUT_UUID;
        uint8_t output_uuid[ESP_UUID_LEN_128] = OUTPUT_UUID;

        if (bt_compare_UUID128(p_data->get_char.char_id.uuid.uuid.uuid128, input_uuid)) {
          printf("found INPUT UUID!\n");
          succes++;
          lego_input_characteristic = p_data->get_char.char_id;
        }

        if (bt_compare_UUID128(p_data->get_char.char_id.uuid.uuid.uuid128, output_uuid)) {
          printf("found OUTPUT UUID!\n");
          succes++;
          lego_output_characteristic = p_data->get_char.char_id;
        }
        if (succes<2){
          esp_ble_gattc_get_characteristic(gattc_if, conn_id, &KEYBLE_application_service, &p_data->get_char.char_id);
        }

        break;
    }

    case ESP_GATTC_GET_DESCR_EVT: {
      printf("get_desc: %x\n", p_data->get_descr.status);

      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        uint16_t notify_en = 0x01;
        printf("REG FOR NOTIFY: status %d", p_data->reg_for_notify.status);
        printf("REG FOR_NOTIFY: srvc_id = %04x, char_id = %04x", p_data->reg_for_notify.srvc_id.id.uuid.uuid.uuid128, p_data->reg_for_notify.char_id.uuid.uuid.uuid128);
        connected = true;
        esp_ble_gattc_write_char_descr(
                gattc_if,
                conn_id,
                &KEYBLE_application_service,
                &p_data->reg_for_notify.char_id,
                &notify_descr_id,
                sizeof(notify_en),
                (uint8_t *)&notify_en,
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE);
/*
                esp_err_t esp_ble_gattc_write_char(
            esp_gatt_if_t gattc_if,
            uint16_t conn_id,
            esp_gatt_srvc_id_t *srvc_id,
            esp_gatt_id_t *char_id,
            uint16_t value_len,
            uint8_t *value,
            esp_gatt_write_type_t write_type,
            esp_gatt_auth_req_t auth_req)*/
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
    {
      printf("recieved notifier 0x");
      for (int i =0; i< sizeof(p_data->notify.value);i++){
      printf("%02x",p_data->notify.value[i]);
      }
      printf("\n");
        printf("NOTIFY: len %d, value %08x \n", p_data->notify.value_len,*(uint32_t *)p_data->notify.value);
        globalHandler(p_data->notify.value,p_data->notify.value_len);

        //WHEN THE BUTTON IS PRESSED

        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
      {
        ESP_LOGI(GATTC_TAG, "WRITE: status %d", p_data->write.status);

        break;
      }
     case ESP_GATTC_WRITE_CHAR_EVT:
     {
        recieved = true;
        ESP_LOGI(GATTC_TAG, "WRITE: status %d", p_data->write.status);

        break;
      }

    }
}



static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  ESP_LOGI(GATTC_TAG, "Received a GAP event: %s", bt_event_type_to_string(event));

  //printf("event: %d\n", event);
    if (event == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
      ESP_LOGI(GATTC_TAG, "status: %d", param->scan_param_cmpl.status);
      ESP_LOGI(GATTC_TAG, "start scanning...");
      uint32_t duration = 10;
      esp_ble_gap_start_scanning(duration);
    } else if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
      ESP_LOGI(GATTC_TAG, "device address (bda): %02x:%02x:%02x:%02x:%02x:%02x", BT_BD_ADDR_HEX(param->scan_rst.bda));
      ESP_LOGI(GATTC_TAG, "device type: %s", bt_dev_type_to_string(param->scan_rst.dev_type));
      ESP_LOGI(GATTC_TAG, "search_evt: %s", bt_gap_search_event_type_to_string(param->scan_rst.search_evt));
      ESP_LOGI(GATTC_TAG, "addr_type: %s", bt_addr_t_to_string(param->scan_rst.ble_addr_type));
      ESP_LOGI(GATTC_TAG, "rssi: %d", param->scan_rst.rssi);
      ESP_LOGI(GATTC_TAG, "flag: %d", param->scan_rst.flag);
      ESP_LOGI(GATTC_TAG, "num_resps: %d", param->scan_rst.num_resps);



      if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
        ESP_LOGI(GATTC_TAG, "payload:");
        uint8_t len;
        uint8_t *data = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &len);
        ESP_LOGI(GATTC_TAG, "len: %d, %.*s", len, len, data);
        ESP_LOGI(GATTC_TAG, "dump -");
        dump_adv_payload(param->scan_rst.ble_adv);


      // if we found our KEY-BLE profile, open the profile and stop scanning
      int const KEYBLE_NAME_LEN = sizeof(wedo_name);
      char const * KEYBLE_NAME = wedo_name;
      if (len == KEYBLE_NAME_LEN) {
        if (strncmp((char*)data, KEYBLE_NAME, len) == 0) {
          connect = true;
          ESP_LOGI(GATTC_TAG, "Connect to the remote device.\n");
          esp_ble_gap_stop_scanning();
          esp_ble_gattc_open(gl_profile_tab[PROFILE_KEYBLE_APP_ID].gattc_if, param->scan_rst.bda, true);
        }
      }



             // esp_ble_gattc_open(gl_profile_tab[PROFILE_B_APP_ID].gattc_if, param->scan_rst.bda, true);

      }
      ESP_LOGI(GATTC_TAG, "");
  }
}


static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    ESP_LOGI(GATTC_TAG, "EVT %d, gattc if %d", event, gattc_if);

     //If event is register event, store the gattc_if for each profile
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(GATTC_TAG, "Reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

     //If the gattc_if equal to profile A, call profile A cb handler,
     //* so here call each profile's callback

        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE ||  /*ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function*/
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }

}


void ble_client_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(GATTC_TAG, "register callback\n");

    //register the scan callback function to the gap moudule
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(GATTC_TAG, "gap register error, error code = %x\n", status);
        return;
    }
    else {
      printf("register callback ok\n");
    }

    esp_err_t ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    ESP_ERROR_CHECK(ret);

    //register the callback function to the gattc module
    if ((status = esp_ble_gattc_register_callback(esp_gattc_cb)) != ESP_OK) {
        ESP_LOGE(GATTC_TAG, "gattc register error, error code = %x\n", status);
        return;
    }


    esp_ble_gattc_app_register(PROFILE_KEYBLE_APP_ID);
    //esp_ble_gattc_app_register(PROFILE_B_APP_ID);
}


void gattc_client_test(void)
{
    esp_bluedroid_init();
    esp_bluedroid_enable();
    ble_client_appRegister();
}
void writeBLECommand(int type, uint8_t* command,int size)
{
/*  while(!recieved){
    delay(10);
  }*/
  recieved = false;
  /*printf("send 0x");
  for (int i =0; i< size;i++){
  printf("%02x",command[i]);
  }
  printf("\n");*/
  if (type == WEDO_INPUT){
    //uint8_t commandIn[] = {0x01, 0x02, 02, 0x23, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01};
    esp_err_t err = esp_ble_gattc_write_char(
                  gl_profile_tab[PROFILE_KEYBLE_APP_ID].gattc_if,
                  gl_profile_tab[PROFILE_KEYBLE_APP_ID].conn_id,
                  &KEYBLE_application_service,
                  &lego_input_characteristic,
                  size,
                  command,
                  ESP_GATT_WRITE_TYPE_RSP,
                  ESP_GATT_AUTH_REQ_NONE);

  if (err != 0){
    printf("Error sending the input message");
  }else{
    //printf("Succesfully sent input message");
  }
  }else{
  esp_err_t err = esp_ble_gattc_write_char(
                gl_profile_tab[PROFILE_KEYBLE_APP_ID].gattc_if,
                gl_profile_tab[PROFILE_KEYBLE_APP_ID].conn_id,
                &KEYBLE_application_service,
                &lego_output_characteristic,
                size,
                command,
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE);
                if (err != 0){
                  printf("Error sending the input message");
                }
              }

}
void setName(const char* name){
  wedo_name = name;
  recieved = true;
}
int getBLEReady(){
  return recieved;
}
int getBLEConnected(){
  return connected;
}
void addBLEhandler(void (*f)(uint8_t*,int)){
  globalHandler = f;
}
