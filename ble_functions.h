#ifndef ble_functions_h
#define ble_functions_h
#include <stdint.h>
#include <Arduino.h>
#include <NimBLEDevice.h>

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

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
// The hub's own physical (green LEGO logo) button - confirmed live: notifies a single
// byte, 0x00/0x01, on release/press. Lives in the WEDO_ADVERTISING_UUID (0x1523)
// service, same as WEDO_PORT_TYPE_UUID below (not WEDO_SERVICE_UUID/0x4f0e, where the
// sensor/output/input characteristics live).
#define WEDO_BUTTON_UUID         "00001526-1212-efde-1523-785feabcd123"
// Reports which device is plugged into which port (attach/detach), separately from
// WEDO_SENSOR_VALUE_UUID which only carries actual sensor readings. Confirmed live on
// real hardware: this characteristic lives in the WEDO_ADVERTISING_UUID (0x1523)
// service, NOT the WEDO_SERVICE_UUID (0x4f0e) service that the other WEDO
// characteristics live in - looking it up under the wrong service is why it was
// missing entirely in earlier discovery attempts.
#define WEDO_PORT_TYPE_UUID      "00001527-1212-efde-1523-785feabcd123"

// LEGO Wireless Protocol 3.x hub service used by Powered Up / BOOST / train hubs.
#define LWP_HUB_SERVICE_UUID         "00001623-1212-efde-1623-785feabcd123"
#define LWP_HUB_CHARACTERISTIC_UUID  "00001624-1212-efde-1623-785feabcd123"

enum BLEHubProtocol {
  BLE_PROTOCOL_UNKNOWN = 0,
  BLE_PROTOCOL_WEDO = 1,
  BLE_PROTOCOL_LWP3 = 2,
};

// How many LEGO devices (hubs, remotes, ...) can be connected to at the same time.
// Defaults to NimBLE's own connection limit - define this yourself before including
// this header to raise it (you'll also need to raise NimBLE's own connection limit to
// match, since this library can't have more simultaneous connections than NimBLE
// allows).
#ifndef WEDO_MAX_CONNECTIONS
#define WEDO_MAX_CONNECTIONS NIMBLE_MAX_CONNECTIONS
#endif

// A handle to one connection "slot" - what bleAcquireSlot() hands back, and what every
// other ble* function takes to say which connection it means. -1 means "no slot".
typedef int8_t BLESlot;
#define BLE_SLOT_INVALID -1

// Which characteristic a notification came from - WEDO 2.0 splits "what's plugged in"
// (PORT_TYPE) from "what value it's reporting" (SENSOR_VALUE) across two different
// characteristics, unlike LWP3 which uses a single one for everything.
enum BLENotificationSource {
  BLE_NOTIFY_SENSOR_VALUE = 0,
  BLE_NOTIFY_PORT_TYPE = 1,
  BLE_NOTIFY_WEDO_BUTTON = 2,
};

// A notification handler gets a bit of context (so the same handler function can serve
// many connections and still tell them apart) plus the raw bytes the hub sent.
typedef void (*BLENotifyHandler)(void* context, uint8_t* data, int size, BLENotificationSource source);

// Forward declarations
class WedoClientCallbacks;
class WedoScanCallbacks;

// One connection's worth of BLE state - every function below is "scoped" to one of
// these via a BLESlot handle, instead of there being a single global connection like
// there used to be. This is what makes it possible to be connected to more than one
// LEGO device (e.g. a hub and a remote control) at the same time.
// Sentinel for BLEConnectionSlot::targetType meaning "any kind of supported LEGO
// device" - matches LegoDeviceType::DEVICE_TYPE_ANY in PoweredUp.h, kept as a raw byte
// here since ble_functions.h doesn't otherwise know about that enum.
#define BLE_DEVICE_TYPE_ANY 0xFF
// Sentinel for "any hub, but never a Remote Control" - matches
// LegoDeviceType::DEVICE_TYPE_ANY_HUB in PoweredUp.h. Needed for sketches that connect
// to a hub and a remote at the same time: an unfiltered BLE_DEVICE_TYPE_ANY hub slot
// could otherwise claim the physical remote before the remote's own (more specific)
// slot gets a chance to, since matching is first-slot-wins with no specificity priority.
#define BLE_DEVICE_TYPE_ANY_HUB 0xFE
// The raw manufacturer-data device-type byte for a Remote Control - matches
// LegoDeviceType::DEVICE_TYPE_POWERED_UP_REMOTE in PoweredUp.h. Used to exclude remotes
// when matching BLE_DEVICE_TYPE_ANY_HUB above.
#define BLE_DEVICE_TYPE_POWERED_UP_REMOTE 0x42

// A write that couldn't be sent immediately because it was attempted from inside a
// notification callback (see bleWriteCommand()/notifyCallback() in ble_functions.cpp) -
// queued here and sent for real from bleHandleConnections() instead, once we're back
// on the main loop. Commands in this protocol are always small (a handful of bytes).
#define BLE_WRITE_QUEUE_SIZE 4
#define BLE_WRITE_MAX_COMMAND_SIZE 24
struct QueuedWrite {
  bool pending = false;
  int type = 0;
  uint8_t data[BLE_WRITE_MAX_COMMAND_SIZE];
  uint8_t size = 0;
};

struct BLEConnectionSlot {
  bool inUse = false;                                   // is this slot claimed by a PoweredUp object?
  const char* targetName = nullptr;                     // only connect to a device with this advertised name (or any, if null)
  uint8_t targetType = BLE_DEVICE_TYPE_ANY;              // only connect to this kind of device (see LegoDeviceType in PoweredUp.h), or any
  const NimBLEAdvertisedDevice* pTargetDevice = nullptr; // set once the scan finds a matching device
  bool doConnect = false;                                // scan callback -> "please connect this slot" signal
  bool connected = false;
  bool ready = true;                                     // true once the last write to this connection has finished
  NimBLEClient* pClient = nullptr;
  NimBLERemoteService* pService = nullptr;
  NimBLERemoteService* pAdvertisingService = nullptr;     // WEDO 2.0 only - 0x1523 service, home of the port type characteristic
  NimBLERemoteCharacteristic* pOutputCharacteristic = nullptr;
  NimBLERemoteCharacteristic* pInputCharacteristic = nullptr;
  NimBLERemoteCharacteristic* pSensorCharacteristic = nullptr;
  NimBLERemoteCharacteristic* pHubCharacteristic = nullptr;
  NimBLERemoteCharacteristic* pPortTypeCharacteristic = nullptr; // WEDO 2.0 only - attach/detach events
  NimBLERemoteCharacteristic* pButtonCharacteristic = nullptr;   // WEDO 2.0 only - hub's own physical button
  BLEHubProtocol protocol = BLE_PROTOCOL_UNKNOWN;
  BLENotifyHandler notifyHandler = nullptr;
  void* notifyContext = nullptr;
  QueuedWrite writeQueue[BLE_WRITE_QUEUE_SIZE];
};

static BLEConnectionSlot bleSlots[WEDO_MAX_CONNECTIONS];
static uint32_t scanTimeMs = 5000;

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

// Claims a free connection slot for a device you want to connect to later (by name,
// by kind, or any supported LEGO device if both are left at their default). Doesn't
// scan or connect yet - that happens in bleConnect(). Returns BLE_SLOT_INVALID if
// every slot is already in use.
BLESlot bleAcquireSlot(const char* name, uint8_t deviceType = BLE_DEVICE_TYPE_ANY);

// Starts scanning (if it isn't already) and blocks until this slot's device is found
// and connected, or timeoutMs milliseconds have passed. Safe to call for more than one
// slot in a row - the scan started by the first call keeps looking for every
// registered slot's device, so a second/third call often returns almost immediately.
void bleConnect(BLESlot slot, uint32_t timeoutMs = 30000);

// Sends a command to this slot's connected device. type is WEDO_INPUT or WEDO_OUTPUT
// (which characteristic to write to - LWP3 devices only have one, so both map to it).
void bleWriteCommand(BLESlot slot, int type, uint8_t* command, int size);

// Registers the function that should be called whenever this slot's device sends a
// notification (sensor data, button presses, attach/detach events, ...). context is
// handed back unchanged as the handler's first argument, so one handler function can
// tell multiple connections apart.
void bleAddNotificationHandler(BLESlot slot, BLENotifyHandler handler, void* context);

bool bleReady(BLESlot slot);
bool bleConnected(BLESlot slot);
BLEHubProtocol bleProtocol(BLESlot slot);

bool connectToWedoServer(BLESlot slot);
void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

// Call this every loop() - it drives background reconnection for every connected slot
// at once. Calling it from more than one object's handleConnection() each loop is
// fine (and expected): it always pumps every slot, not just the caller's.
void bleHandleConnections();

#endif
