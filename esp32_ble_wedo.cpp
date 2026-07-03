#include "esp32_ble_wedo.h"

#define LWP_IMMEDIATE_NO_ACK 0x10
#define LWP_PORT_OUTPUT_COMMAND 0x81
#define LWP_WRITE_DIRECT_MODE_DATA 0x51
#define LWP_EXTERNAL_PORT_A 0x00
#define LWP_INTERNAL_LED_PORT 0x32
#define LWP_HUB_ATTACHED_IO 0x04
#define LWP_PORT_INPUT_FORMAT_SETUP_SINGLE 0x41
#define LWP_PORT_VALUE_SINGLE 0x45
#define LWP_PORT_INFORMATION_REQUEST 0x21
#define LWP_PORT_INFORMATION 0x43
#define LWP_PORT_MODE_INFORMATION_REQUEST 0x22
#define LWP_PORT_MODE_INFORMATION 0x44
// IO Type ID of the RGB Light / Hub LED device, used to find which port it's attached to -
// this varies per hub model (e.g. 50 on the train hub, 52 on the Remote Control), so it
// can't be a fixed port constant.
#define ID_RGB_LIGHT 0x0017

// Port Information Request "Information Type" values (see LWP3 docs, Port Information Request).
#define PORT_INFO_MODE_INFO 0x01

// Port Mode Information Request "Mode Information Type" values.
#define MODE_INFO_NAME 0x00
#define MODE_INFO_RAW 0x01
#define MODE_INFO_PCT 0x02
#define MODE_INFO_SI 0x03
#define MODE_INFO_SYMBOL 0x04
#define MODE_INFO_VALUE_FORMAT 0x80

static uint8_t _translateLwpPort(uint8_t wedo_port) {
  if (wedo_port >= 1 && wedo_port <= WEDO_PORTS) {
    return wedo_port - 1;
  }

  return wedo_port;
}

static int _writeLwpCommand(uint8_t port, uint8_t mode, const uint8_t* payload, uint8_t payload_size) {
  const uint8_t header_size = 7;
  uint8_t command[header_size + payload_size];

  command[0] = header_size + payload_size;
  command[1] = 0x00;
  command[2] = LWP_PORT_OUTPUT_COMMAND;
  command[3] = port;
  command[4] = LWP_IMMEDIATE_NO_ACK;
  command[5] = LWP_WRITE_DIRECT_MODE_DATA;
  command[6] = mode;

  for (uint8_t index = 0; index < payload_size; index++) {
    command[header_size + index] = payload[index];
  }

  writeBLECommand(WEDO_OUTPUT, command, sizeof(command));
  return 1;
}

static void _sendPortInputFormatSetup(uint8_t port, uint8_t mode) {
  // Port Input Format Setup (Single): enable notifications for this port/mode.
  // Delta interval of 1 means "notify on every change".
  uint8_t command[] = {0x0A, 0x00, LWP_PORT_INPUT_FORMAT_SETUP_SINGLE, port, mode,
                        0x01, 0x00, 0x00, 0x00, 0x01};
  writeBLECommand(WEDO_OUTPUT, command, sizeof(command));
}

// The hub LED's port number varies per hub model, so it's detected from the Hub Attached
// I/O announcement (IO Type ID ID_RGB_LIGHT) rather than assumed fixed. Defaults to the
// train hub's port until an attach event says otherwise.
static uint8_t _ledPort = LWP_INTERNAL_LED_PORT;

// The hub LED only acts on WriteDirectModeData for whichever mode it's currently switched
// into (COL=0 or RGB=1) - a Port Input Format Setup has to select that mode first, or writes
// to the other mode are silently ignored. Track the active mode so we only resend the
// switch when it actually changes.
static int8_t _ledActiveMode = -1;

// Last RGB/index colour actually written, so repeated writeRGB()/writeIndexColor() calls
// with an unchanged colour (e.g. called every loop() iteration) don't re-send identical
// commands - flooding the hub with a write every ~50ms was overloading the Remote
// Control's BLE stack and causing it to drop the connection.
static int16_t _lastRGB[3] = {-1, -1, -1};
static int16_t _lastIndexColor = -1;

// A reconnect (to the same or a different hub) invalidates everything the LED code
// assumed about the previous connection - which port the LED is on, what mode it's in,
// and what colour was last sent - since the new connection starts fresh.
static bool _wasConnected = false;

static void _resetLedCacheOnReconnect() {
  bool isConnected = getBLEConnected();
  if (isConnected && !_wasConnected) {
    _ledPort = LWP_INTERNAL_LED_PORT;
    _ledActiveMode = -1;
    _lastRGB[0] = _lastRGB[1] = _lastRGB[2] = -1;
    _lastIndexColor = -1;
  }
  _wasConnected = isConnected;
}

static void _setLedMode(uint8_t mode) {
  if (_ledActiveMode != mode) {
    _sendPortInputFormatSetup(_ledPort, mode);
    _ledActiveMode = mode;
  }
}

// --- Port/Port Mode Information discovery ---------------------------------------------
// Whenever a device attaches (on connect, or later), queries and prints every mode it
// supports as a TSV table, using Port Information Request (0x21) and Port Mode Information
// Request (0x22): https://lego.github.io/lego-ble-wireless-protocol-docs/index.html#port-mode-information-request
// One port is discovered at a time, one mode-info field at a time, all sent from
// handleConnection() (never from the notification callback - see the re-arm comment above).

#define DISCOVERY_QUEUE_SIZE 8
#define DISCOVERY_STEP_TIMEOUT_MS 500

enum DiscoveryStep {
  DISCOVERY_STEP_IDLE = 0,
  DISCOVERY_STEP_PORT_INFO,
  DISCOVERY_STEP_NAME,
  DISCOVERY_STEP_RAW,
  DISCOVERY_STEP_PCT,
  DISCOVERY_STEP_SI,
  DISCOVERY_STEP_SYMBOL,
  DISCOVERY_STEP_VALUE_FORMAT,
};

static uint8_t _discoveryQueue[DISCOVERY_QUEUE_SIZE];
static uint8_t _discoveryQueueCount = 0;

static uint8_t _discoveryStep = DISCOVERY_STEP_IDLE;
static uint8_t _discoveryPort = 0;
static uint8_t _discoveryTotalModes = 0;
static uint16_t _discoveryInputModes = 0;
static uint16_t _discoveryOutputModes = 0;
static uint8_t _discoveryMode = 0;
static bool _discoveryStepComplete = false;
static unsigned long _discoveryStepSentAt = 0;

// Fields gathered for the mode currently being discovered.
static char _modeName[12];
static float _modeRawMin, _modeRawMax;
static float _modePctMin, _modePctMax;
static float _modeSiMin, _modeSiMax;
static char _modeSymbol[6];
static uint8_t _modeNumDatasets, _modeDatasetType, _modeFigures, _modeDecimals;
static bool _haveName, _haveRaw, _havePct, _haveSi, _haveSymbol, _haveValueFormat;

static void _queuePortDiscovery(uint8_t port) {
  if (_discoveryQueueCount < DISCOVERY_QUEUE_SIZE) {
    _discoveryQueue[_discoveryQueueCount++] = port;
  }
}

static void _sendPortInformationRequest(uint8_t port, uint8_t infoType) {
  uint8_t command[] = {0x05, 0x00, LWP_PORT_INFORMATION_REQUEST, port, infoType};
  writeBLECommand(WEDO_OUTPUT, command, sizeof(command));
}

static void _sendPortModeInformationRequest(uint8_t port, uint8_t mode, uint8_t infoType) {
  uint8_t command[] = {0x06, 0x00, LWP_PORT_MODE_INFORMATION_REQUEST, port, mode, infoType};
  writeBLECommand(WEDO_OUTPUT, command, sizeof(command));
}

static void _printModeRow() {
  bool isInput = (_discoveryInputModes & (1 << _discoveryMode)) != 0;
  bool isOutput = (_discoveryOutputModes & (1 << _discoveryMode)) != 0;
  static const char* datasetTypeNames[] = {"8-bit", "16-bit", "32-bit", "float"};
  const char* datasetType = (_haveValueFormat && _modeDatasetType < 4) ? datasetTypeNames[_modeDatasetType] : "";

  printf("%d\t%d\t%s\t%s\t%s\t", _discoveryPort, _discoveryMode, _haveName ? _modeName : "",
         isInput ? "yes" : "no", isOutput ? "yes" : "no");
  if (_haveRaw) printf("%g\t%g\t", _modeRawMin, _modeRawMax); else printf("\t\t");
  if (_havePct) printf("%g\t%g\t", _modePctMin, _modePctMax); else printf("\t\t");
  if (_haveSi) printf("%g\t%g\t", _modeSiMin, _modeSiMax); else printf("\t\t");
  printf("%s\t", _haveSymbol ? _modeSymbol : "");
  if (_haveValueFormat) {
    printf("%d\t%s\t%d\t%d\n", _modeNumDatasets, datasetType, _modeFigures, _modeDecimals);
  } else {
    printf("\t\t\t\n");
  }
}

static void _startModeQuery() {
  _haveName = _haveRaw = _havePct = _haveSi = _haveSymbol = _haveValueFormat = false;
  _discoveryStep = DISCOVERY_STEP_NAME;
  _sendPortModeInformationRequest(_discoveryPort, _discoveryMode, MODE_INFO_NAME);
  _discoveryStepSentAt = millis();
}

// Advances the discovery state machine by one step. Called every loop() iteration via
// handleConnection(); a no-op unless a response just arrived (_discoveryStepComplete) or
// the current step has timed out (some ports/modes don't answer every query).
static void _advanceDiscovery() {
  if (_discoveryStep == DISCOVERY_STEP_IDLE) {
    if (_discoveryQueueCount == 0) {
      return;
    }
    _discoveryPort = _discoveryQueue[0];
    for (uint8_t i = 1; i < _discoveryQueueCount; i++) {
      _discoveryQueue[i - 1] = _discoveryQueue[i];
    }
    _discoveryQueueCount--;

    printf("Port mode information for port %d:\n", _discoveryPort);
    printf("Port\tMode\tName\tInput\tOutput\tRAW min\tRAW max\tPCT min\tPCT max\tSI min\tSI max\tSymbol\tDatasets\tType\tFigures\tDecimals\n");
    _discoveryStep = DISCOVERY_STEP_PORT_INFO;
    _sendPortInformationRequest(_discoveryPort, PORT_INFO_MODE_INFO);
    _discoveryStepSentAt = millis();
    return;
  }

  bool timedOut = (millis() - _discoveryStepSentAt) > DISCOVERY_STEP_TIMEOUT_MS;
  if (!_discoveryStepComplete && !timedOut) {
    return;
  }
  _discoveryStepComplete = false;

  switch (_discoveryStep) {
    case DISCOVERY_STEP_PORT_INFO:
      if (_discoveryTotalModes == 0) {
        _discoveryStep = DISCOVERY_STEP_IDLE;
        return;
      }
      _discoveryMode = 0;
      _startModeQuery();
      break;
    case DISCOVERY_STEP_NAME:
      _discoveryStep = DISCOVERY_STEP_RAW;
      _sendPortModeInformationRequest(_discoveryPort, _discoveryMode, MODE_INFO_RAW);
      _discoveryStepSentAt = millis();
      break;
    case DISCOVERY_STEP_RAW:
      _discoveryStep = DISCOVERY_STEP_PCT;
      _sendPortModeInformationRequest(_discoveryPort, _discoveryMode, MODE_INFO_PCT);
      _discoveryStepSentAt = millis();
      break;
    case DISCOVERY_STEP_PCT:
      _discoveryStep = DISCOVERY_STEP_SI;
      _sendPortModeInformationRequest(_discoveryPort, _discoveryMode, MODE_INFO_SI);
      _discoveryStepSentAt = millis();
      break;
    case DISCOVERY_STEP_SI:
      _discoveryStep = DISCOVERY_STEP_SYMBOL;
      _sendPortModeInformationRequest(_discoveryPort, _discoveryMode, MODE_INFO_SYMBOL);
      _discoveryStepSentAt = millis();
      break;
    case DISCOVERY_STEP_SYMBOL:
      _discoveryStep = DISCOVERY_STEP_VALUE_FORMAT;
      _sendPortModeInformationRequest(_discoveryPort, _discoveryMode, MODE_INFO_VALUE_FORMAT);
      _discoveryStepSentAt = millis();
      break;
    case DISCOVERY_STEP_VALUE_FORMAT:
      _printModeRow();
      _discoveryMode++;
      if (_discoveryMode >= _discoveryTotalModes) {
        _discoveryStep = DISCOVERY_STEP_IDLE;
      } else {
        _startModeQuery();
      }
      break;
    default:
      _discoveryStep = DISCOVERY_STEP_IDLE;
  }
}

static void _LWP3notificationHandler(uint8_t* data, int size){
  if (size < 3) {
    return;
  }

  uint8_t messageType = data[2];

  if (messageType == LWP_HUB_ATTACHED_IO) {
    if (size < 5) {
      return;
    }

    uint8_t port = data[3];
    uint8_t event = data[4];

    if (event == 0x01 && size >= 7) {
      // Attached I/O: IO Type ID identifies which sensor/motor is plugged into this port.
      uint16_t ioTypeId = data[5] | (data[6] << 8);
      printf("LWP3 device attached on port %d, IO type ID: 0x%04x\n", port, ioTypeId);

      if (ioTypeId == ID_RGB_LIGHT && port != _ledPort) {
        _ledPort = port;
        _ledActiveMode = -1; // force the next write to (re)select its mode on the new port
      }

      // Print every mode this port supports as soon as it attaches - every sensor,
      // actuator, internal or external. The actual query writes happen later in
      // handleConnection(), same reason as the re-arm above.
      _queuePortDiscovery(port);

      // The hub drops a port's input format subscription whenever its device detaches,
      // so flag it for re-arming if the user previously subscribed via setPortInputFormat().
      // The actual re-subscribe write happens later in handleConnection(), not here -
      // writing to the BLE characteristic from within this notification callback exhausts
      // the NimBLE stack's buffer pool.
      if (port < WEDO_PORTS && devices[port] == ID_LWP3_GENERIC_SENSOR && portHandlers[port] != nullptr) {
        portReArmPending[port] = true;
      }
    } else if (event == 0x00) {
      printf("LWP3 device detached from port %d\n", port);
    }
    return;
  }

  if (messageType == LWP_PORT_VALUE_SINGLE) {
    if (size < 5) {
      return;
    }

    uint8_t port = data[3];
    if (port >= WEDO_PORTS || devices[port] == 0) {
      return;
    }

    int valueSize = size - 4;
    int8_t values[valueSize];
    for (int index = 0; index < valueSize; index++) {
      values[index] = (int8_t)data[4 + index];
    }
    portHandlers[port](values, valueSize);
    return;
  }

  if (messageType == LWP_PORT_INFORMATION) {
    if (size < 6) {
      return;
    }
    uint8_t port = data[3];
    uint8_t infoType = data[4];
    if (infoType == PORT_INFO_MODE_INFO && port == _discoveryPort &&
        _discoveryStep == DISCOVERY_STEP_PORT_INFO && size >= 11) {
      _discoveryTotalModes = data[6];
      _discoveryInputModes = data[7] | (data[8] << 8);
      _discoveryOutputModes = data[9] | (data[10] << 8);
      _discoveryStepComplete = true;
    }
    return;
  }

  if (messageType == LWP_PORT_MODE_INFORMATION) {
    if (size < 6) {
      return;
    }
    uint8_t port = data[3];
    uint8_t mode = data[4];
    uint8_t infoType = data[5];
    if (port != _discoveryPort || mode != _discoveryMode) {
      return; // not the response we're currently waiting for
    }

    if (infoType == MODE_INFO_NAME && _discoveryStep == DISCOVERY_STEP_NAME) {
      int len = size - 6;
      if (len > 11) len = 11;
      if (len < 0) len = 0;
      memcpy(_modeName, data + 6, len);
      _modeName[len] = 0;
      _haveName = true;
      _discoveryStepComplete = true;
    } else if (infoType == MODE_INFO_RAW && _discoveryStep == DISCOVERY_STEP_RAW && size >= 14) {
      memcpy(&_modeRawMin, data + 6, 4);
      memcpy(&_modeRawMax, data + 10, 4);
      _haveRaw = true;
      _discoveryStepComplete = true;
    } else if (infoType == MODE_INFO_PCT && _discoveryStep == DISCOVERY_STEP_PCT && size >= 14) {
      memcpy(&_modePctMin, data + 6, 4);
      memcpy(&_modePctMax, data + 10, 4);
      _havePct = true;
      _discoveryStepComplete = true;
    } else if (infoType == MODE_INFO_SI && _discoveryStep == DISCOVERY_STEP_SI && size >= 14) {
      memcpy(&_modeSiMin, data + 6, 4);
      memcpy(&_modeSiMax, data + 10, 4);
      _haveSi = true;
      _discoveryStepComplete = true;
    } else if (infoType == MODE_INFO_SYMBOL && _discoveryStep == DISCOVERY_STEP_SYMBOL) {
      int len = size - 6;
      if (len > 5) len = 5;
      if (len < 0) len = 0;
      memcpy(_modeSymbol, data + 6, len);
      _modeSymbol[len] = 0;
      _haveSymbol = true;
      _discoveryStepComplete = true;
    } else if (infoType == MODE_INFO_VALUE_FORMAT && _discoveryStep == DISCOVERY_STEP_VALUE_FORMAT && size >= 10) {
      _modeNumDatasets = data[6];
      _modeDatasetType = data[7];
      _modeFigures = data[8];
      _modeDecimals = data[9];
      _haveValueFormat = true;
      _discoveryStepComplete = true;
    }
  }
}

static void _WEDOnotificationHandler(uint8_t* data, int size){
  if (getBLEProtocol() == BLE_PROTOCOL_LWP3) {
    _LWP3notificationHandler(data, size);
    return;
  }

  if (getBLEProtocol() != BLE_PROTOCOL_WEDO) {
    return;
  }

  if (size < 3) {
    printf("Invalid data size: %d\n", size);
    return;
  }
  
  uint8_t value = data[2];
  uint8_t port = data[1];

  printf("value: %d ; port: %d\n", data[2], data[1]);
  
  if (port > 0 && port <= WEDO_PORTS){
    if (devices[port-1] > 0){
      printf("Sent command to the handler\n");
      if (devices[port-1] == ID_DETECT_SENSOR)
      {
        //send only one value (0-100)
        int8_t callback_data[] = {(int8_t)data[2]};
        portHandlers[port-1](callback_data, 1);
      }
      else if (devices[port-1] == ID_TILT_SENSOR)
      {
        //send two values (-45 -> 45)
        if (size >= 4) {
          int8_t callback_data[] = {(int8_t)data[2], (int8_t)data[3]};
          portHandlers[port-1](callback_data, 2);
        }
      }
    } else {
      printf("Can't handle the message - perhaps you didn't use setDetectSensor()\n");
    }
  }
}

Wedo::Wedo(const char* name)
{
  setName(name);
  addBLEhandler(_WEDOnotificationHandler);
}

int Wedo::connect(){
  // Initialize BT with NimBLE
  gattc_client_test();
  return 1;
}

int Wedo::writeOutputCommand(uint8_t* command, int size)
{
  /*
  Write an output command to the output characteristic of the WEDO
  */
  // Make a safety buffer so there won't be any loss of messages due to sending two messages at the same time
  // It waits till the last message was sent with a maximum of 100ms
  for(int i = 0; i < 20 && !ready(); i++){
    delay(5);
  }
  writeBLECommand(WEDO_OUTPUT, command, size);
  return 1;
}

int Wedo::writeInputCommand(uint8_t input_command[], int size)
{
  /*
  Write an input command to the input characteristic of the WEDO
  */
  // Make a safety buffer so there won't be any loss of messages due to sending two messages at the same time
  // It waits till the last message was sent with a maximum of 100ms
  for(int i = 0; i < 20 && !ready(); i++){
    delay(5);
  }
  printf("writeInputCommand with len: %d \n", size);
  writeBLECommand(WEDO_INPUT, input_command, size);
  return 1;
}

void Wedo::writeMotor(uint8_t wedo_port, int wedo_speed)
{
  // From http://www.ev3dev.org/docs/tutorials/controlling-wedo2-motor/
  if (getBLEProtocol() == BLE_PROTOCOL_LWP3) {
    uint8_t speed_byte = static_cast<uint8_t>(wedo_speed);
    uint8_t payload[] = {speed_byte};
    _writeLwpCommand(_translateLwpPort(wedo_port), 0x00, payload, sizeof(payload));
    return;
  }

  // conversion from int (both pos and neg) to unsigned 8 bit int
  uint8_t speed_byte = wedo_speed;
  uint8_t command[] = {wedo_port, 0x01, 0x01, speed_byte};
  writeOutputCommand(command, sizeof(command));
}

void Wedo::writeIndexColor(uint8_t color){
  if (getBLEProtocol() == BLE_PROTOCOL_LWP3) {
    _resetLedCacheOnReconnect();
    if (_ledActiveMode == 0x00 && _lastIndexColor == color) {
      return; // unchanged since last write - avoid flooding the hub with redundant writes
    }
    _setLedMode(0x00);
    uint8_t payload[] = {color};
    _writeLwpCommand(_ledPort, 0x00, payload, sizeof(payload));
    _lastIndexColor = color;
    return;
  }

  // From http://ofalcao.pt/blog/2016/wedo-2-0-colors-with-python
  uint8_t command[] = {0x06, 0x04, 0x01, color};
  writeOutputCommand(command, sizeof(command));
}

void Wedo::writeRGB(uint8_t red, uint8_t green, uint8_t blue){
  if (getBLEProtocol() == BLE_PROTOCOL_LWP3) {
    _resetLedCacheOnReconnect();
    if (_ledActiveMode == 0x01 && _lastRGB[0] == red && _lastRGB[1] == green && _lastRGB[2] == blue) {
      return; // unchanged since last write - avoid flooding the hub with redundant writes
    }
    _setLedMode(0x01);
    uint8_t payload[] = {red, green, blue};
    _writeLwpCommand(_ledPort, 0x01, payload, sizeof(payload));
    _lastRGB[0] = red;
    _lastRGB[1] = green;
    _lastRGB[2] = blue;
    return;
  }

  uint8_t command[] = {0x06, 0x04, 0x03, red, green, blue};
  writeOutputCommand(command, sizeof(command));
}

void Wedo::writeSound(unsigned int frequency, unsigned int length){
  if (getBLEProtocol() == BLE_PROTOCOL_LWP3) {
    printf("writeSound is only supported for WEDO hubs\n");
    return;
  }

  // From https://github.com/vheun/wedo2/blob/master/index.js (setSound)
  uint8_t command[] = {
    0x05,
    0x02,
    0x04,
    uint8_t((frequency >> (8*0)) & 0xff),
    uint8_t((frequency >> (8*1)) & 0xff),
    uint8_t((length >> (8*0)) & 0xff),
    uint8_t((length >> (8*1)) & 0xff)
  };
  writeOutputCommand(command, sizeof(command));
}

boolean Wedo::connected(){
  return getBLEConnected();
}

boolean Wedo::ready(){
  return getBLEReady();
}

void Wedo::handleConnection(){
  // Call the background connection handler
  handleBLEConnection();

  // Re-arm any LWP3 port input subscriptions flagged by the notification callback.
  // Done here (outside the BLE callback) since a GATT write from within the notification
  // callback exhausts the NimBLE stack's buffer pool.
  for (uint8_t port = 0; port < WEDO_PORTS; port++) {
    if (portReArmPending[port]) {
      portReArmPending[port] = false;
      _sendPortInputFormatSetup(port, portModes[port]);
    }
  }

  // Advance the port mode discovery/print state machine, same reason as above.
  if (getBLEProtocol() == BLE_PROTOCOL_LWP3) {
    _advanceDiscovery();
  }
}

void Wedo::setRGBMode(){
  if (getBLEProtocol() == BLE_PROTOCOL_LWP3) {
    _setLedMode(0x01);
    return;
  }

  if (getBLEProtocol() != BLE_PROTOCOL_WEDO) {
    return;
  }

  writePortDefinition(0x06, 0x17, 0x01, 0x02);
}

void Wedo::setIndexMode(){
  if (getBLEProtocol() == BLE_PROTOCOL_LWP3) {
    _setLedMode(0x00);
    return;
  }

  if (getBLEProtocol() != BLE_PROTOCOL_WEDO) {
    return;
  }

  writePortDefinition(0x06, 0x17, 0x00, 0x00);
}

void Wedo::setDetectSensor(uint8_t port, inputHandlerFunction portHandler){
  if (getBLEProtocol() != BLE_PROTOCOL_WEDO) {
    printf("setDetectSensor is only supported for WEDO hubs\n");
    return;
  }

  writePortDefinition(port, ID_DETECT_SENSOR, 0, RANGE_100);
  devices[port-1] = ID_DETECT_SENSOR;
  portHandlers[port-1] = portHandler;
}

void Wedo::setTiltSensor(uint8_t port, inputHandlerFunction portHandler){
  if (getBLEProtocol() != BLE_PROTOCOL_WEDO) {
    printf("setTiltSensor is only supported for WEDO hubs\n");
    return;
  }

  writePortDefinition(port, ID_TILT_SENSOR, 0, RANGE_100);
  devices[port-1] = ID_TILT_SENSOR;
  portHandlers[port-1] = portHandler;
}

void Wedo::setPortInputFormat(uint8_t wedo_port, uint8_t mode, inputHandlerFunction portHandler){
  if (getBLEProtocol() != BLE_PROTOCOL_LWP3) {
    printf("setPortInputFormat is only supported for LEGO Powered Up / BOOST / train hubs\n");
    return;
  }

  uint8_t port = _translateLwpPort(wedo_port);

  devices[port] = ID_LWP3_GENERIC_SENSOR;
  portHandlers[port] = portHandler;
  portModes[port] = mode;
  _sendPortInputFormatSetup(port, mode);
}

void Wedo::writePortDefinition(uint8_t port, uint8_t type, uint8_t mode, uint8_t format){
  if (getBLEProtocol() != BLE_PROTOCOL_WEDO) {
    printf("writePortDefinition is only supported for WEDO hubs\n");
    return;
  }

  uint8_t command[] = {0x01, 0x02, port, type, mode, 0x01, 0x00, 0x00, 0x00, format, 0x01};
  printf("writePortDefinition with len: %d \n", sizeof(command));
  writeInputCommand(command, sizeof(command));
}

void Wedo::addNotificationHandler(void (*f)(uint8_t*, int)){
  // Overrule the standard notification handler
  addBLEhandler(f);
}
