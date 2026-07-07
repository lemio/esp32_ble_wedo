/*
@Author Geert Roumen
@Date 5-6-2017
@Hardware
https://www.analoglamb.com/product/esp32-development-board/
@library
https://github.com/lemio/esp32_PoweredUp
@Status
Working
@licence
https://creativecommons.org/licenses/by-sa/4.0/

Connect the motor to any port on a LEGO hub (WeDo 2.0, Powered Up, BOOST, train, Duplo)
and the button to pin 0 on the ESP32.

When the button is pressed,
the motor will run and the LED will turn green (red when released).
*/

#include <PoweredUp.h>

// DEVICE_TYPE_ANY_HUB connects to any supported LEGO hub, but never a Remote Control -
// so this can't accidentally connect to a nearby remote instead of an actual hub.
PoweredUp myHub(nullptr, DEVICE_TYPE_ANY_HUB);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  myHub.connect();
  //Wait untill the wedo is connected to the ESP32
  while (!myHub.connected()){
    Serial.print(".");
    delay(100);
    }
}

void loop() {
  // Handle automatic reconnection if connection is lost
  myHub.handleConnection();
  
  //read the value from the button (on pin 0)
  boolean value = !digitalRead(0);
  //invert that value (HIGH->false and LOW->true),
  //so that if the button is pressed (LOW)
  //the motor and LED will shine

  //convert the boolean to a value that makes sense
  //for the LED (green while pressed, red while released)
  myHub.writeIndexColor(value ? LEGO_COLOR_GREEN : LEGO_COLOR_RED);
  //write a value to a motor connected
  myHub.writeMotor(value*100);
}
