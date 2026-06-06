#include "esp32_ble_wedo.h"

#define LWP_IMMEDIATE_NO_ACK 0x10
#define LWP_PORT_OUTPUT_COMMAND 0x81
#define LWP_WRITE_DIRECT_MODE_DATA 0x51
#define LWP_EXTERNAL_PORT_A 0x00
#define LWP_INTERNAL_LED_PORT 0x32

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

static void _WEDOnotificationHandler(uint8_t* data, int size){
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
    uint8_t payload[] = {color};
    _writeLwpCommand(LWP_INTERNAL_LED_PORT, 0x00, payload, sizeof(payload));
    return;
  }

  // From http://ofalcao.pt/blog/2016/wedo-2-0-colors-with-python
  uint8_t command[] = {0x06, 0x04, 0x01, color};
  writeOutputCommand(command, sizeof(command));
}

void Wedo::writeRGB(uint8_t red, uint8_t green, uint8_t blue){
  if (getBLEProtocol() == BLE_PROTOCOL_LWP3) {
    uint8_t payload[] = {red, green, blue};
    _writeLwpCommand(LWP_INTERNAL_LED_PORT, 0x01, payload, sizeof(payload));
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
}

void Wedo::setRGBMode(){
  if (getBLEProtocol() != BLE_PROTOCOL_WEDO) {
    return;
  }

  writePortDefinition(0x06, 0x17, 0x01, 0x02);
}

void Wedo::setIndexMode(){
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
