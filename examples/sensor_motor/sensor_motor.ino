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

#include <PoweredUp.h>

//Make myWedo object
PoweredUp myHub;
//Add a global variable detectSensorValue
int detectSensorValue = 0;
//Make the handleInput function (try to avoid using wedo
//functions in this one) and keep it short
void handleInput(int8_t* value,int size){
    detectSensorValue = value[0];
  }

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  myHub.connect();
  //Wait untill the wedo is connected to the ESP32
  while (!myHub.connected()){
    Serial.print(".");
    delay(100);
    }
    delay(3000);
    myHub.monitorDistance(2,handleInput);
}


void loop() {
  delay(100);
  myHub.writeIndexColor(detectSensorValue/10);
  delay(100);
  myHub.writeMotor(1,100-detectSensorValue);
}
