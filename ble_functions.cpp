#include "ble_functions.h"

#define GATTC_TAG "WEDO_BLE"

// Callback instances
static WedoClientCallbacks clientCallbacks;
static WedoScanCallbacks scanCallbacks;

// Notification callback handler
void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.printf("Notification from %s: ", pRemoteCharacteristic->getUUID().toString().c_str());
    Serial.printf("Length: %d, Data: 0x", length);
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02x", pData[i]);
    }
    Serial.printf("\n");
    
    // Call the user-defined handler if set
    if (globalHandler != nullptr) {
        globalHandler(pData, length);
    }
}

// Client callback implementations
void WedoClientCallbacks::onConnect(NimBLEClient* pClient) {
    Serial.printf("Connected to WEDO device\n");
    connected = true;
}

void WedoClientCallbacks::onDisconnect(NimBLEClient* pClient, int reason) {
    Serial.printf("Disconnected from WEDO, reason = %d - Starting scan\n", reason);
    connected = false;
    doConnect = false;
    pWedoClient = nullptr;
    pWedoService = nullptr;
    pOutputCharacteristic = nullptr;
    pInputCharacteristic = nullptr;
    pSensorCharacteristic = nullptr;
    
    // Restart scanning after a short delay
    delay(1000);
    if (NimBLEDevice::getScan()->isScanning()) {
        NimBLEDevice::getScan()->stop();
    }
    NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    Serial.printf("Reconnection scan started\n");
}

// Scan callback implementations
void WedoScanCallbacks::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    Serial.printf("Advertised Device: %s\n", advertisedDevice->toString().c_str());
    
    // Check if this is the WEDO device - it advertises with UUID 00001523 but service is 00004F0E
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(WEDO_ADVERTISING_UUID))) {
        Serial.printf("Found WEDO Advertising Service\n");
        
        // Check if the name matches (if a name filter is set)
        if (wedo_name != nullptr) {
            if (advertisedDevice->haveName()) {
                std::string deviceName = advertisedDevice->getName();
                if (deviceName == wedo_name) {
                    Serial.printf("Found our WEDO device: %s\n", wedo_name);
                    NimBLEDevice::getScan()->stop();
                    pTargetDevice = advertisedDevice;
                    doConnect = true;
                }
            }
        } else {
            // No name filter, connect to any WEDO device
            Serial.printf("Found a WEDO device\n");
            NimBLEDevice::getScan()->stop();
            pTargetDevice = advertisedDevice;
            doConnect = true;
        }
    }
}

void WedoScanCallbacks::onScanEnd(const NimBLEScanResults& results, int reason) {
    Serial.printf("Scan Ended, reason: %d, device count: %d\n", reason, results.getCount());
    
    // Only restart if we haven't found a device to connect to
    if (!doConnect && !connected) {
        Serial.printf("Restarting scan...\n");
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
}

// Connect to WEDO server
bool connectToWedoServer() {
    if (pTargetDevice == nullptr) {
        Serial.printf("No target device set\n");
        return false;
    }
    
    // Check if we have a client we can reuse
    if (NimBLEDevice::getCreatedClientCount()) {
        pWedoClient = NimBLEDevice::getClientByPeerAddress(pTargetDevice->getAddress());
        if (pWedoClient) {
            if (!pWedoClient->connect(pTargetDevice, false)) {
                Serial.printf("Reconnect failed\n");
                return false;
            }
            Serial.printf("Reconnected client\n");
        } else {
            pWedoClient = NimBLEDevice::getDisconnectedClient();
        }
    }
    
    // Create a new client if needed
    if (!pWedoClient) {
        if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.printf("Max clients reached\n");
            return false;
        }
        
        pWedoClient = NimBLEDevice::createClient();
        Serial.printf("New client created\n");
        
        pWedoClient->setClientCallbacks(&clientCallbacks, false);
        pWedoClient->setConnectionParams(12, 12, 0, 150);
        pWedoClient->setConnectTimeout(5 * 1000);
        
        if (!pWedoClient->connect(pTargetDevice)) {
            NimBLEDevice::deleteClient(pWedoClient);
            pWedoClient = nullptr;
            Serial.printf("Failed to connect\n");
            return false;
        }
    }
    
    if (!pWedoClient->isConnected()) {
        if (!pWedoClient->connect(pTargetDevice)) {
            Serial.printf("Failed to connect\n");
            return false;
        }
    }
    
    Serial.printf("Connected to: %s RSSI: %d\n", 
                  pWedoClient->getPeerAddress().toString().c_str(), 
                  pWedoClient->getRssi());
    
    // Get the WEDO service
    pWedoService = pWedoClient->getService(WEDO_SERVICE_UUID);
    if (pWedoService == nullptr) {
        Serial.printf("Failed to find WEDO service\n");
        pWedoClient->disconnect();
        return false;
    }
    
    Serial.printf("Found WEDO service\n");
    
    // Get the characteristics
    pSensorCharacteristic = pWedoService->getCharacteristic(WEDO_SENSOR_VALUE_UUID);
    pOutputCharacteristic = pWedoService->getCharacteristic(WEDO_OUTPUT_UUID);
    pInputCharacteristic = pWedoService->getCharacteristic(WEDO_INPUT_UUID);
    
    if (pSensorCharacteristic == nullptr || pOutputCharacteristic == nullptr || pInputCharacteristic == nullptr) {
        Serial.printf("Failed to find required characteristics\n");
        Serial.printf("Sensor: %s, Output: %s, Input: %s\n",
                      pSensorCharacteristic ? "OK" : "FAIL",
                      pOutputCharacteristic ? "OK" : "FAIL",
                      pInputCharacteristic ? "OK" : "FAIL");
        pWedoClient->disconnect();
        return false;
    }
    
    Serial.printf("Found all required characteristics\n");
    
    // Subscribe to notifications on the sensor characteristic
    if (pSensorCharacteristic->canNotify()) {
        if (!pSensorCharacteristic->subscribe(true, notifyCallback)) {
            Serial.printf("Failed to subscribe to notifications\n");
            pWedoClient->disconnect();
            return false;
        }
        Serial.printf("Subscribed to sensor notifications\n");
    }
    
    connected = true;
    Serial.printf("WEDO device ready!\n");
    return true;
}

// Initialize BLE and start scanning
void gattc_client_test(void) {
    Serial.printf("Initializing NimBLE Client\n");
    
    // Initialize NimBLE
    NimBLEDevice::init("ESP32-WEDO");
    
    // Optional: Set transmit power
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9dBm */
    
    // Get the scan object
    NimBLEScan* pScan = NimBLEDevice::getScan();
    
    // Set scan callbacks
    pScan->setScanCallbacks(&scanCallbacks, false);
    
    // Set scan parameters
    pScan->setInterval(100);
    pScan->setWindow(100);
    pScan->setActiveScan(true);
    
    // Start scanning
    pScan->start(scanTimeMs, false, true);
    Serial.printf("Scanning for WEDO devices...\n");
    
    // Non-blocking connection handling - will be called in loop
    // Check periodically if we need to connect
    unsigned long startTime = millis();
    while (!connected && (millis() - startTime < 30000)) { // 30 second timeout
        if (doConnect) {
            doConnect = false;
            if (connectToWedoServer()) {
                Serial.printf("Successfully connected to WEDO\n");
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
    
    if (!connected) {
        Serial.printf("Warning: Initial connection attempt timed out. Scan continues in background.\n");
    }
}

// Handle background connection attempts
void handleBLEConnection() {
    if (!connected && doConnect) {
        doConnect = false;
        if (connectToWedoServer()) {
            Serial.printf("Successfully reconnected to WEDO\n");
        } else {
            Serial.printf("Reconnection attempt failed\n");
            // Restart scanning if not already scanning
            if (!NimBLEDevice::getScan()->isScanning()) {
                NimBLEDevice::getScan()->start(scanTimeMs, false, true);
            }
        }
    }
}

// Write a command to the WEDO device
void writeBLECommand(int type, uint8_t* command, int size) {
    if (!connected || pWedoClient == nullptr) {
        Serial.printf("Not connected to WEDO device - attempting reconnection\n");
        
        // Try to trigger reconnection if not already scanning
        if (!NimBLEDevice::getScan()->isScanning() && !doConnect) {
            Serial.printf("Starting reconnection scan\n");
            NimBLEDevice::getScan()->start(scanTimeMs, false, true);
        }
        
        recieved = true;
        return;
    }
    
    recieved = false;
    
    NimBLERemoteCharacteristic* pChar = nullptr;
    
    if (type == WEDO_INPUT) {
        pChar = pInputCharacteristic;
    } else if (type == WEDO_OUTPUT) {
        pChar = pOutputCharacteristic;
    }
    
    if (pChar == nullptr) {
        Serial.printf("Characteristic not available\n");
        recieved = true;
        return;
    }
    
    if (pChar->canWrite()) {
        if (pChar->writeValue(command, size, true)) {
            // Serial.printf("Successfully wrote command\n");
            recieved = true;
        } else {
            Serial.printf("Failed to write command\n");
            recieved = true;
        }
    } else {
        Serial.printf("Characteristic not writable\n");
        recieved = true;
    }
}

// Set the device name to scan for
void setName(const char* name) {
    if (name != nullptr && name[0] == '\0') {
        wedo_name = nullptr;
    } else {
        wedo_name = name;
    }
    recieved = true;
}

// Check if the device is ready to receive commands
int getBLEReady() {
    return recieved;
}

// Check if the device is connected
int getBLEConnected() {
    return connected;
}

// Add a notification handler
void addBLEhandler(void (*f)(uint8_t*, int)) {
    globalHandler = f;
}
