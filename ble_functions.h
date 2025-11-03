#ifndef ble_functions_h
#define ble_functions_h
#include <stdint.h>
#include <Arduino.h>
#include <NimBLEDevice.h>

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

//#define KEYBLE_UUID_APPL_SRVC { 0x23,0xd1,0xbc,0xea,0x5f,0x78,0x23,0x15,0xde,0xef, 0x12, 0x12,0x23, 0x15 ,0x00,0x00} This is the nordic button service
/*
Define the UUID's to listen to
  -> Service is the main LEGO i/o service
  -> MOTOR_UUID is the characteristic for the output commands
*/

#define WEDO_INPUT 0
#define WEDO_OUTPUT 1

// WEDO 2.0 Service and Characteristic UUIDs
// Note: The advertising service UUID is 00001523-... but the actual service is 00004F0E-...
#define WEDO_SERVICE_UUID        "00004f0e-1212-efde-1523-785feabcd123"  // Main LEGO I/O service
#define WEDO_ADVERTISING_UUID    "00001523-1212-efde-1523-785feabcd123"  // Used in advertising
#define WEDO_SENSOR_VALUE_UUID   "00001560-1212-efde-1523-785feabcd123"
#define WEDO_OUTPUT_UUID         "00001565-1212-efde-1523-785feabcd123"
#define WEDO_INPUT_UUID          "00001563-1212-efde-1523-785feabcd123"
#define WEDO_BATTERY_UUID        "00001526-1212-efde-1523-785feabcd123"

// Forward declarations
class WedoClientCallbacks;
class WedoScanCallbacks;

// NimBLE client and service pointers
static NimBLEClient* pWedoClient = nullptr;
static NimBLERemoteService* pWedoService = nullptr;
static NimBLERemoteCharacteristic* pOutputCharacteristic = nullptr;
static NimBLERemoteCharacteristic* pInputCharacteristic = nullptr;
static NimBLERemoteCharacteristic* pSensorCharacteristic = nullptr;

static const NimBLEAdvertisedDevice* pTargetDevice = nullptr;
static bool doConnect = false;
static bool connected = false;
static bool recieved = true;
static uint32_t scanTimeMs = 5000;

static void (*globalHandler)(uint8_t*,int) = nullptr;
static const char* wedo_name = nullptr;

// Callback classes
class WedoClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient, int reason) override;
};

class WedoScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onScanEnd(const NimBLEScanResults& results, int reason) override;
};

// Function declarations
void gattc_client_test(void);
void writeBLECommand(int type, uint8_t* command, int size);
void addBLEhandler(void (*f)(uint8_t*,int));
void setName(const char* name);
int getBLEReady();
int getBLEConnected();
bool connectToWedoServer();
void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
void handleBLEConnection();  // New function to handle background reconnection

#endif
