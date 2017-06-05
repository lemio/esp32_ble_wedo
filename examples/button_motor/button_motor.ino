/*
@Author Geert Roumen
@Date 5-6-2017
@Hardware
https://www.analoglamb.com/product/esp32-development-board/
@library
https://github.com/lemio/esp32_ble_wedo
@Status
Working (but probably sub-optimal/stable)
@licence
https://creativecommons.org/licenses/by-sa/4.0/

if you look in front of the WEDO ports,
so the back of the wedo, on port 1 there
is a LEGO wedo2.0 motor connected
 _________________
|  port2 | port1  |
|________|________|
|                 |
|                 |
|_________________|
*/

#include <esp32_ble_wedo.h>

Wedo myWedo("test");

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  myWedo.connect();
  //Wait untill the wedo is connected to the ESP32
  while (!myWedo.connected()){
    Serial.print(".");
    delay(100);
    }
}

void loop() {
  //read the value from the button (on pin 0)
  boolean value = !digitalRead(0);
  //invert that value (HIGH->false and LOW->true),
  //so that if the button is pressed (LOW)
  //the motor and LED will shine

  //convert the boolean to a value that makes sense
  //for the LED (white -> 10 in this case)
  myWedo.writeIndexColor(value*LEGO_COLOR_WHITE);
  //write a value to the motor on port 1
  myWedo.writeMotor(1,value*100);
}
