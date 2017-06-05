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
at port 2 there should be a infrared
detect sensor
 _________________
|  port2 | port1  |
|________|________|
|                 |
|                 |
|_________________|
*/

#include <esp32_ble_wedo.h>
//declare the handleInput before calling the Wedo Class
void handleInput(int value);
//Make myWedo object
Wedo myWedo("test",handleInput);
//Add a global variable detectSensorValue
int detectSensorValue = 0;
//Make the handleInput function (try to avoid using wedo
//functions in this one) and keep it short
void handleInput(int value){
    detectSensorValue = value;
  }

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  myWedo.connect();
  //Wait untill the wedo is connected to the ESP32
  while (!myWedo.connected()){
    Serial.print(".");
    delay(100);
    }
    delay(3000);
    myWedo.setDetectSensor(2);
}


void loop() {
  delay(100);
  myWedo.writeIndexColor(detectSensorValue);
  delay(100);
  myWedo.writeMotor(1,100-detectSensorValue*10);
}
