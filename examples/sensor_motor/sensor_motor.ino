/*
@Author Geert Roumen
@Date 5-6-2017
@Hardware
https://www.analoglamb.com/product/esp32-development-board/
@library
https://github.com/lemio/esp32_ble_wedo
@Status
Working
@licence
https://creativecommons.org/licenses/by-sa/4.0/

A motor and a detect (infrared distance) sensor plugged into a LEGO hub (WeDo 2.0,
Powered Up, BOOST, train, Duplo), either way round - onDistanceChanged() with no port
given finds the sensor itself, wherever it's plugged in.
*/

#include <PoweredUp.h>

// Connect to any LEGO hub (WeDo 2.0, Powered Up, BOOST, train, Duplo) - the first one found.
PoweredUp myHub(nullptr, DEVICE_TYPE_ANY_HUB);
int detectSensorValue = 0;

void setup() {
  Serial.begin(115200);
  myHub.connect();
  while (!myHub.connected()){
    Serial.print(".");
    delay(100);
    }
    delay(3000);
    // No port given - finds the distance sensor itself, whichever port it's plugged into.
    myHub.onDistanceChanged([](int8_t distance){
      detectSensorValue = distance;
    });
}


void loop() {
  // Handle automatic reconnection if connection is lost
  myHub.handleConnection();

  delay(100);
  // detectSensorValue is 0-10 on both WeDo 2.0 and Powered Up/BOOST hubs, matching the
  // LEGO_COLOR_* index range directly.
  myHub.writeIndexColor(detectSensorValue);
  delay(100);
  // No port given - finds the motor itself, whichever port it's plugged into. Scaled up
  // to the full -100..100 motor range since detectSensorValue only goes 0-10.
  myHub.writeMotor(100 - detectSensorValue * 10);
}
