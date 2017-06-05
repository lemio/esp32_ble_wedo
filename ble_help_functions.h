#include <esp_bt_defs.h>

void dump_adv_payload(uint8_t *payload);
const char *bt_event_type_to_string(uint32_t eventType);
const char *bt_gap_search_event_type_to_string(uint32_t searchEvt);
const char *bt_addr_t_to_string(esp_ble_addr_type_t type);
const char *bt_dev_type_to_string(esp_bt_dev_type_t type);

int bt_compare_UUID128(uint8_t uuid1[ESP_UUID_LEN_128], uint8_t uuid2[ESP_UUID_LEN_128]);
int bt_compare_UUID128_reversed(uint8_t uuid1[ESP_UUID_LEN_128], uint8_t uuid2[ESP_UUID_LEN_128]);
#define GATTC_TAG "GATTC_DEMO"
