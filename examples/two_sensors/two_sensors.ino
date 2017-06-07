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
is a LEGO detect sensor connected
at port 2 there should be a tilt sensor
 _________________
|  port2 | port1  |
|________|________|
|                 |
|                 |
|_________________|
*/

#include <esp32_ble_wedo.h>
//Make myWedo object
Wedo myWedo("test");
//Add a global variable detectSensorValue
int detectSensorValue = 0;
//Make the handleInput functions (try to avoid using wedo
//functions in this one) and keep it short
void handleTiltSensor(int8_t* value,int size){
    printf("\n\n \tx: %i y: %i \n\n",value[0],value[1]);
  }
void handleDetectSensor(int8_t* value, int size){
    printf("\n\n \tRIGHT: %i \n\n",value[0]);
    detectSensorValue = value[0];
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
    delay(1000);
    myWedo.setTiltSensor(2,handleTiltSensor);
    myWedo.setDetectSensor(1,handleDetectSensor);
    myWedo.setRGBMode();
}


void loop() {
  if (detectSensorValue != 0){
    myWedo.writeSound(detectSensorValue*10,200);
    myWedo.writeRGB(detectSensorValue*2.5,0,255-detectSensorValue*2.5);
  }
}
