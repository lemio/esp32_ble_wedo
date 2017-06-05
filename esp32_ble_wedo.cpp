#include "esp32_ble_wedo.h"

Wedo::Wedo(const char* name)
{
  setName(name);
}
int Wedo::connect(){
  //return recieved;
  if (btStart()) {
    gattc_client_test();
  }
  return 1;
}
int Wedo::writeOutputCommand(uint8_t* command)
{
  /*
  Write an output command to the output characteristic of the WEDO
  */
  //recieved = false;
  //Make a safety buffer so there won't be any loss of messages due to sending two messages at the same time
  //It waits till the last message was sent with a maximum of 100ms
  for(int i=0;i<20&&!ready();i++){
    delay(5);
  }
  writeBLECommand(command);
}

void Wedo::writeMotor(uint8_t wedo_port,int wedo_speed)
{
  //From http://www.ev3dev.org/docs/tutorials/controlling-wedo2-motor/
  //conversion from int (both pos and neg) to unsigned 8 bit int
  uint8_t speed_byte = wedo_speed;
  uint8_t command[] = {wedo_port,0x01,0x01,speed_byte};
  writeOutputCommand(command);
}
void Wedo::writeIndexColor(uint8_t color){
    //From http://ofalcao.pt/blog/2016/wedo-2-0-colors-with-python
    uint8_t command[] = {0x06,0x04,0x01,color};
    writeOutputCommand(command);
}
void Wedo::writeSound(unsigned int frequency, unsigned int length){
    //From https://github.com/vheun/wedo2/blob/master/index.js (setSound)

    uint8_t command[] = {
      0x05,
      0x02,
      0x04,
      uint8_t (frequency >> (8*0)) & 0xff,
      uint8_t (frequency >> (8*1)) & 0xff,
      uint8_t (length >> (8*0)) & 0xff,
      uint8_t (length >> (8*1)) & 0xff
    };
    writeOutputCommand(command);
}
boolean Wedo::connected(){
  return getBLEConnected();
}
boolean Wedo::ready(){
  return getBLEReady();
}
/*
int Wedo::isReady(){

}*/
