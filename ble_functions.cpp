#include "ble_functions.h"

#define LEGO_COMPANY_ID_LSB 0x97
#define LEGO_COMPANY_ID_MSB 0x03

// Reads the "System Type + Device Number" byte out of a LEGO device's manufacturer
// data, if it has one recognized as a supported LEGO device. This byte is exactly what
// LegoDeviceType's constants (PoweredUp.h) hold, so it can be compared directly against
// a slot's requested device type.
static bool getLegoManufacturerDeviceType(const NimBLEAdvertisedDevice* advertisedDevice, uint8_t* outType) {
    if (!advertisedDevice->haveManufacturerData()) {
        return false;
    }

    std::string manufacturerData = advertisedDevice->getManufacturerData();
    if (manufacturerData.length() < 4) {
        return false;
    }

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(manufacturerData.data());
    if (bytes[0] != LEGO_COMPANY_ID_LSB || bytes[1] != LEGO_COMPANY_ID_MSB) {
        return false;
    }

    uint8_t systemTypeAndDevice = bytes[3];
    uint8_t systemType = systemTypeAndDevice >> 5;
    uint8_t deviceNumber = systemTypeAndDevice & 0x1F;

    bool isWedoHub = systemType == 0 && deviceNumber == 0;
    bool isDuploTrain = systemType == 1 && deviceNumber == 0;
    bool isLegoSystemHub = systemType == 2 && deviceNumber <= 2;

    if (!isWedoHub && !isDuploTrain && !isLegoSystemHub) {
        return false;
    }

    *outType = systemTypeAndDevice;
    return true;
}

// Callback instances
static WedoClientCallbacks clientCallbacks;
static WedoScanCallbacks scanCallbacks;
static bool bleStackInitialized = false;

// True for the duration of a notification handler call. Writing to the BLE stack from
// inside that callback (whichever connection it's for) exhausts NimBLE's buffer pool -
// bleWriteCommand() checks this and queues the write instead when it's set.
static bool bleInNotificationCallback = false;

static void _sendNow(BLESlot slot, int type, uint8_t* command, int size);

static BLESlot findSlotByClient(NimBLEClient* pClient) {
    for (BLESlot i = 0; i < WEDO_MAX_CONNECTIONS; i++) {
        if (bleSlots[i].inUse && bleSlots[i].pClient == pClient) {
            return i;
        }
    }
    return BLE_SLOT_INVALID;
}

// Notification callback handler - routes every incoming notification to whichever
// slot's connection it actually came from, then to that slot's own handler. Doesn't log
// the raw bytes itself (that fires on every single sensor reading - PoweredUp's own
// port/mode discovery output is the useful, low-noise summary of what's actually there).
void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    BLESlot slot = findSlotByClient(pRemoteCharacteristic->getClient());
    if (slot == BLE_SLOT_INVALID) {
        return;
    }

    BLENotificationSource source = BLE_NOTIFY_SENSOR_VALUE;
    if (pRemoteCharacteristic == bleSlots[slot].pPortTypeCharacteristic) {
        source = BLE_NOTIFY_PORT_TYPE;
    } else if (pRemoteCharacteristic == bleSlots[slot].pButtonCharacteristic) {
        source = BLE_NOTIFY_WEDO_BUTTON;
    }

    if (bleSlots[slot].notifyHandler != nullptr) {
        bleInNotificationCallback = true;
        bleSlots[slot].notifyHandler(bleSlots[slot].notifyContext, pData, length, source);
        bleInNotificationCallback = false;
    }
}

// Client callback implementations
void WedoClientCallbacks::onConnect(NimBLEClient* pClient) {
    Serial.printf("Connected to WEDO device\n");
    BLESlot slot = findSlotByClient(pClient);
    if (slot != BLE_SLOT_INVALID) {
        bleSlots[slot].connected = true;
    }
}

void WedoClientCallbacks::onDisconnect(NimBLEClient* pClient, int reason) {
    Serial.printf("Disconnected from WEDO, reason = %d - Starting scan\n", reason);

    BLESlot slot = findSlotByClient(pClient);
    if (slot != BLE_SLOT_INVALID) {
        BLEConnectionSlot& s = bleSlots[slot];
        s.connected = false;
        s.doConnect = false;
        s.protocol = BLE_PROTOCOL_UNKNOWN;
        s.pClient = nullptr;
        s.pService = nullptr;
        s.pAdvertisingService = nullptr;
        s.pOutputCharacteristic = nullptr;
        s.pInputCharacteristic = nullptr;
        s.pSensorCharacteristic = nullptr;
        s.pHubCharacteristic = nullptr;
        s.pPortTypeCharacteristic = nullptr;
        s.pButtonCharacteristic = nullptr;
        s.pTargetDevice = nullptr;
    }

    // Restart scanning after a short delay so this slot's device can be found again
    delay(1000);
    if (NimBLEDevice::getScan()->isScanning()) {
        NimBLEDevice::getScan()->stop();
    }
    NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    Serial.printf("Reconnection scan started\n");
}

// Scan callback implementations - doesn't log every device seen while scanning (that's
// every nearby BLE device, not just LEGO ones - very high volume); only the ones that
// turn out to be a supported LEGO device, below.
void WedoScanCallbacks::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    bool isWedoDevice = advertisedDevice->isAdvertisingService(NimBLEUUID(WEDO_ADVERTISING_UUID));
    uint8_t deviceType = 0;
    bool haveDeviceType = getLegoManufacturerDeviceType(advertisedDevice, &deviceType);
    bool isLwpHub = advertisedDevice->isAdvertisingService(NimBLEUUID(LWP_HUB_SERVICE_UUID)) || haveDeviceType;

    if (!isWedoDevice && !isLwpHub) {
        return;
    }

    if (isWedoDevice && !haveDeviceType) {
        // Identified via the WEDO service UUID rather than manufacturer data, but that
        // alone already tells us what kind of device this is.
        deviceType = 0x00; // DEVICE_TYPE_WEDO_HUB
        haveDeviceType = true;
    }

    Serial.printf("Found %s advertising service\n", isWedoDevice ? "WEDO" : "LEGO Hub");

    // Has some other slot already claimed this exact device? A device keeps advertising
    // repeatedly while we wait to actually connect to it, so without this check the same
    // physical device could end up matched to two different unnamed slots.
    for (BLESlot i = 0; i < WEDO_MAX_CONNECTIONS; i++) {
        BLEConnectionSlot& slot = bleSlots[i];
        if (slot.inUse && (slot.doConnect || slot.connected) && slot.pTargetDevice != nullptr &&
            slot.pTargetDevice->getAddress() == advertisedDevice->getAddress()) {
            return; // already being connected to by another slot
        }
    }

    // Check this device against every slot that's still looking for one. More than one
    // slot can be waiting at once (e.g. a hub and a remote both mid-connect), so a
    // single advertisement is compared against all of them - but only ONE slot may
    // claim it, first match wins, so the same device is never assigned twice.
    for (BLESlot i = 0; i < WEDO_MAX_CONNECTIONS; i++) {
        BLEConnectionSlot& slot = bleSlots[i];
        if (!slot.inUse || slot.doConnect || slot.connected) {
            continue; // not waiting for a device
        }

        bool nameMatches = slot.targetName == nullptr;
        if (!nameMatches && advertisedDevice->haveName()) {
            nameMatches = advertisedDevice->getName() == slot.targetName;
        }

        bool typeMatches = slot.targetType == BLE_DEVICE_TYPE_ANY ||
                            (slot.targetType == BLE_DEVICE_TYPE_ANY_HUB && haveDeviceType &&
                             deviceType != BLE_DEVICE_TYPE_POWERED_UP_REMOTE) ||
                            (haveDeviceType && slot.targetType == deviceType);

        if (nameMatches && typeMatches) {
            Serial.printf("Found a supported LEGO device for connection slot %d\n", i);
            slot.pTargetDevice = advertisedDevice;
            slot.doConnect = true;
            break; // this device is claimed - don't also hand it to another slot
        }
    }

    // Once every registered slot has something to connect to, there's nothing left to
    // scan for right now - stop so the radio can spend its time connecting instead.
    bool everySlotClaimed = true;
    for (BLESlot i = 0; i < WEDO_MAX_CONNECTIONS; i++) {
        if (bleSlots[i].inUse && !bleSlots[i].doConnect && !bleSlots[i].connected) {
            everySlotClaimed = false;
            break;
        }
    }
    if (everySlotClaimed) {
        NimBLEDevice::getScan()->stop();
    }
}

void WedoScanCallbacks::onScanEnd(const NimBLEScanResults& results, int reason) {
    Serial.printf("Scan Ended, reason: %d, device count: %d\n", reason, results.getCount());

    bool anySlotStillWaiting = false;
    for (BLESlot i = 0; i < WEDO_MAX_CONNECTIONS; i++) {
        if (bleSlots[i].inUse && !bleSlots[i].doConnect && !bleSlots[i].connected) {
            anySlotStillWaiting = true;
            break;
        }
    }

    if (anySlotStillWaiting) {
        Serial.printf("Restarting scan...\n");
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
}

// Connect to a slot's target device once the scan has found it
bool connectToWedoServer(BLESlot slot) {
    BLEConnectionSlot& s = bleSlots[slot];

    if (s.pTargetDevice == nullptr) {
        Serial.printf("No target device set\n");
        return false;
    }

    // Check if we have a client we can reuse
    if (NimBLEDevice::getCreatedClientCount()) {
        s.pClient = NimBLEDevice::getClientByPeerAddress(s.pTargetDevice->getAddress());
        if (s.pClient) {
            if (!s.pClient->connect(s.pTargetDevice, false)) {
                Serial.printf("Reconnect failed\n");
                return false;
            }
            Serial.printf("Reconnected client\n");
        } else {
            s.pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    // Create a new client if needed
    if (!s.pClient) {
        if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.printf("Max clients reached\n");
            return false;
        }

        s.pClient = NimBLEDevice::createClient();
        Serial.printf("New client created\n");

        s.pClient->setClientCallbacks(&clientCallbacks, false);
        // A tight 15ms interval with only a 1.5s supervision timeout was causing
        // spurious "reason=520" (connection timeout) disconnects during normal use.
        // These still showed up occasionally with two connections active at once (a hub
        // + a remote sharing the ESP32's single BLE radio) even after loosening this
        // once already, so the interval/timeout are loosened further here: a wider
        // interval gives the radio more slack to service both links without missing
        // connection events, and a generous supervision timeout means an occasional
        // missed event doesn't cascade into a full disconnect/reconnect (which is also
        // what was resetting the remote's LED back to its idle colour mid-test).
        // Peripheral latency (allowing the peripheral to skip connection events) was
        // tried too but caused WeDo 2.0 hubs specifically to report spurious rapid
        // detach/attach notifications with nothing physically unplugged - reverted to 0.
        s.pClient->setConnectionParams(30, 60, 0, 600);
        s.pClient->setConnectTimeout(5 * 1000);

        if (!s.pClient->connect(s.pTargetDevice)) {
            NimBLEDevice::deleteClient(s.pClient);
            s.pClient = nullptr;
            Serial.printf("Failed to connect\n");
            return false;
        }
    }

    if (!s.pClient->isConnected()) {
        if (!s.pClient->connect(s.pTargetDevice)) {
            Serial.printf("Failed to connect\n");
            return false;
        }
    }

    Serial.printf("Connected to: %s RSSI: %d\n",
                  s.pClient->getPeerAddress().toString().c_str(),
                  s.pClient->getRssi());

    // Get the WEDO service
    s.pService = s.pClient->getService(WEDO_SERVICE_UUID);
    if (s.pService != nullptr) {
        s.protocol = BLE_PROTOCOL_WEDO;
        Serial.printf("Found WEDO service\n");

        // Force a full characteristic discovery pass up front, rather than looking each
        // UUID up individually - some hubs' extra characteristics (like port type,
        // below) weren't reliably found by per-UUID lookups otherwise.
        s.pService->getCharacteristics(true);

        // Get the characteristics
        s.pSensorCharacteristic = s.pService->getCharacteristic(WEDO_SENSOR_VALUE_UUID);
        s.pOutputCharacteristic = s.pService->getCharacteristic(WEDO_OUTPUT_UUID);
        s.pInputCharacteristic = s.pService->getCharacteristic(WEDO_INPUT_UUID);

        if (s.pSensorCharacteristic == nullptr || s.pOutputCharacteristic == nullptr || s.pInputCharacteristic == nullptr) {
            Serial.printf("Failed to find required WEDO characteristics\n");
            Serial.printf("Sensor: %s, Output: %s, Input: %s\n",
                          s.pSensorCharacteristic ? "OK" : "FAIL",
                          s.pOutputCharacteristic ? "OK" : "FAIL",
                          s.pInputCharacteristic ? "OK" : "FAIL");
            s.pClient->disconnect();
            return false;
        }

        Serial.printf("Found all required WEDO characteristics\n");

        if (s.pSensorCharacteristic->canNotify()) {
            if (!s.pSensorCharacteristic->subscribe(true, notifyCallback)) {
                Serial.printf("Failed to subscribe to WEDO notifications\n");
                s.pClient->disconnect();
                return false;
            }
            Serial.printf("Subscribed to WEDO notifications\n");
        }

        // Attach/detach events (what's plugged into which port) come through the port
        // type characteristic, which lives in a *different* GATT service (0x1523) than
        // the sensor/output/input characteristics above (0x4f0e). Not fatal if missing
        // - older firmware or a slightly different hub might not have it.
        s.pAdvertisingService = s.pClient->getService(WEDO_ADVERTISING_UUID);
        s.pPortTypeCharacteristic = s.pAdvertisingService != nullptr
                                         ? s.pAdvertisingService->getCharacteristic(WEDO_PORT_TYPE_UUID)
                                         : nullptr;
        if (s.pPortTypeCharacteristic == nullptr) {
            Serial.printf("WEDO port type characteristic not found on this hub - attach/detach detection unavailable\n");
        } else if (!s.pPortTypeCharacteristic->canNotify()) {
            Serial.printf("WEDO port type characteristic found but can't notify\n");
        } else if (s.pPortTypeCharacteristic->subscribe(true, notifyCallback)) {
            Serial.printf("Subscribed to WEDO port type notifications\n");
        } else {
            Serial.printf("Failed to subscribe to WEDO port type notifications\n");
        }

        // The hub's own physical button - same 0x1523 service as the port type
        // characteristic above.
        s.pButtonCharacteristic = s.pAdvertisingService != nullptr
                                       ? s.pAdvertisingService->getCharacteristic(WEDO_BUTTON_UUID)
                                       : nullptr;
        if (s.pButtonCharacteristic == nullptr) {
            Serial.printf("WEDO button characteristic not found on this hub - hub button unavailable\n");
        } else if (!s.pButtonCharacteristic->canNotify()) {
            Serial.printf("WEDO button characteristic found but can't notify\n");
        } else if (s.pButtonCharacteristic->subscribe(true, notifyCallback)) {
            Serial.printf("Subscribed to WEDO button notifications\n");
        } else {
            Serial.printf("Failed to subscribe to WEDO button notifications\n");
        }
    } else {
        s.pService = s.pClient->getService(LWP_HUB_SERVICE_UUID);
        if (s.pService == nullptr) {
            Serial.printf("Failed to find a supported LEGO service\n");
            s.pClient->disconnect();
            return false;
        }

        s.protocol = BLE_PROTOCOL_LWP3;
        Serial.printf("Found LEGO Hub service\n");

        s.pHubCharacteristic = s.pService->getCharacteristic(LWP_HUB_CHARACTERISTIC_UUID);
        if (s.pHubCharacteristic == nullptr) {
            Serial.printf("Failed to find LEGO Hub characteristic\n");
            s.pClient->disconnect();
            return false;
        }

        s.pOutputCharacteristic = s.pHubCharacteristic;
        s.pInputCharacteristic = s.pHubCharacteristic;
        s.pSensorCharacteristic = s.pHubCharacteristic;

        if (s.pHubCharacteristic->canNotify()) {
            if (!s.pHubCharacteristic->subscribe(true, notifyCallback)) {
                Serial.printf("Failed to subscribe to LEGO Hub notifications\n");
                s.pClient->disconnect();
                return false;
            }
            Serial.printf("Subscribed to LEGO Hub notifications\n");
        }
    }

    s.connected = true;
    Serial.printf("WEDO device ready!\n");
    return true;
}

BLESlot bleAcquireSlot(const char* name, uint8_t deviceType) {
    for (BLESlot i = 0; i < WEDO_MAX_CONNECTIONS; i++) {
        if (!bleSlots[i].inUse) {
            bleSlots[i] = BLEConnectionSlot(); // reset to defaults
            bleSlots[i].inUse = true;
            bleSlots[i].targetName = (name != nullptr && name[0] != '\0') ? name : nullptr;
            bleSlots[i].targetType = deviceType;
            return i;
        }
    }
    Serial.printf("No free connection slots available (WEDO_MAX_CONNECTIONS = %d)\n", WEDO_MAX_CONNECTIONS);
    return BLE_SLOT_INVALID;
}

void bleConnect(BLESlot slot, uint32_t timeoutMs) {
    if (slot == BLE_SLOT_INVALID) {
        return;
    }

    if (!bleStackInitialized) {
        Serial.printf("Initializing NimBLE Client\n");
        NimBLEDevice::init("ESP32-WEDO");
        NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9dBm */

        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setScanCallbacks(&scanCallbacks, false);
        pScan->setInterval(100);
        pScan->setWindow(100);
        pScan->setActiveScan(true);
        pScan->start(scanTimeMs, false, true);
        Serial.printf("Scanning for supported LEGO devices...\n");

        bleStackInitialized = true;
    } else if (!NimBLEDevice::getScan()->isScanning() && !bleSlots[slot].connected) {
        // The scan may already have stopped (e.g. every previously-registered slot got
        // claimed) - make sure this newly-registered slot's device gets looked for too.
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }

    unsigned long startTime = millis();
    while (!bleSlots[slot].connected && (millis() - startTime < timeoutMs)) {
        if (bleSlots[slot].doConnect) {
            bleSlots[slot].doConnect = false;
            if (connectToWedoServer(slot)) {
                Serial.printf("Successfully connected to LEGO device\n");
                break;
            } else {
                Serial.printf("Failed to connect, continuing scan\n");
                delay(1000);
                if (!NimBLEDevice::getScan()->isScanning()) {
                    NimBLEDevice::getScan()->start(scanTimeMs, false, true);
                }
            }
        }
        delay(100);
    }

    if (!bleSlots[slot].connected) {
        Serial.printf("Warning: Initial connection attempt timed out. Scan continues in background.\n");
    }
}

// Handle background connection attempts for every slot at once
void bleHandleConnections() {
    for (BLESlot i = 0; i < WEDO_MAX_CONNECTIONS; i++) {
        if (bleSlots[i].inUse && !bleSlots[i].connected && bleSlots[i].doConnect) {
            bleSlots[i].doConnect = false;
            if (connectToWedoServer(i)) {
                Serial.printf("Successfully reconnected to LEGO device\n");
            } else {
                Serial.printf("Reconnection attempt failed\n");
                if (!NimBLEDevice::getScan()->isScanning()) {
                    NimBLEDevice::getScan()->start(scanTimeMs, false, true);
                }
            }
        }
    }

    // Flush any writes that got queued because they were attempted from inside a
    // notification callback - safe to send for real now that we're on the main loop.
    for (BLESlot i = 0; i < WEDO_MAX_CONNECTIONS; i++) {
        if (!bleSlots[i].inUse) {
            continue;
        }
        for (uint8_t q = 0; q < BLE_WRITE_QUEUE_SIZE; q++) {
            if (bleSlots[i].writeQueue[q].pending) {
                bleSlots[i].writeQueue[q].pending = false;
                _sendNow(i, bleSlots[i].writeQueue[q].type, bleSlots[i].writeQueue[q].data,
                         bleSlots[i].writeQueue[q].size);
            }
        }
    }
}

// The actual write - only ever called when we know we're not inside a notification
// callback (either bleWriteCommand() called it directly, or bleHandleConnections() is
// flushing something that got queued earlier).
static void _sendNow(BLESlot slot, int type, uint8_t* command, int size) {
    BLEConnectionSlot& s = bleSlots[slot];

    if (!s.connected || s.pClient == nullptr) {
        Serial.printf("Not connected to WEDO device - attempting reconnection\n");

        if (!NimBLEDevice::getScan()->isScanning() && !s.doConnect) {
            Serial.printf("Starting reconnection scan\n");
            NimBLEDevice::getScan()->start(scanTimeMs, false, true);
        }

        s.ready = true;
        return;
    }

    s.ready = false;

    NimBLERemoteCharacteristic* pChar = nullptr;

    if (type == WEDO_INPUT) {
        pChar = s.protocol == BLE_PROTOCOL_LWP3 ? s.pHubCharacteristic : s.pInputCharacteristic;
    } else if (type == WEDO_OUTPUT) {
        pChar = s.protocol == BLE_PROTOCOL_LWP3 ? s.pHubCharacteristic : s.pOutputCharacteristic;
    }

    if (pChar == nullptr) {
        Serial.printf("Characteristic not available\n");
        s.ready = true;
        return;
    }

    if (pChar->canWrite()) {
        if (pChar->writeValue(command, size, true)) {
            s.ready = true;
        } else {
            Serial.printf("Failed to write command\n");
            s.ready = true;
        }
    } else {
        Serial.printf("Characteristic not writable\n");
        s.ready = true;
    }
}

void bleWriteCommand(BLESlot slot, int type, uint8_t* command, int size) {
    if (slot == BLE_SLOT_INVALID) {
        return;
    }

    if (bleInNotificationCallback) {
        // Writing to the BLE stack from inside a notification callback - even for a
        // different connection than the one the notification came from - exhausts
        // NimBLE's buffer pool. Queue it and send it for real from
        // bleHandleConnections() once we're back on the main loop. This is what makes
        // it safe to e.g. drive a hub's motor directly from a remote's button handler.
        BLEConnectionSlot& s = bleSlots[slot];
        for (uint8_t i = 0; i < BLE_WRITE_QUEUE_SIZE; i++) {
            if (!s.writeQueue[i].pending) {
                s.writeQueue[i].pending = true;
                s.writeQueue[i].type = type;
                s.writeQueue[i].size = size > BLE_WRITE_MAX_COMMAND_SIZE ? BLE_WRITE_MAX_COMMAND_SIZE : size;
                memcpy(s.writeQueue[i].data, command, s.writeQueue[i].size);
                return;
            }
        }
        Serial.printf("Write queue full for slot %d - dropping a command\n", slot);
        return;
    }

    _sendNow(slot, type, command, size);
}

void bleAddNotificationHandler(BLESlot slot, BLENotifyHandler handler, void* context) {
    if (slot == BLE_SLOT_INVALID) {
        return;
    }
    bleSlots[slot].notifyHandler = handler;
    bleSlots[slot].notifyContext = context;
}

bool bleReady(BLESlot slot) {
    if (slot == BLE_SLOT_INVALID) {
        return true;
    }
    return bleSlots[slot].ready;
}

bool bleConnected(BLESlot slot) {
    if (slot == BLE_SLOT_INVALID) {
        return false;
    }
    return bleSlots[slot].connected;
}

BLEHubProtocol bleProtocol(BLESlot slot) {
    if (slot == BLE_SLOT_INVALID) {
        return BLE_PROTOCOL_UNKNOWN;
    }
    return bleSlots[slot].protocol;
}
