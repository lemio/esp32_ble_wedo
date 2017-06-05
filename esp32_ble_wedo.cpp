#include "esp32_ble_wedo.h"

Wedo::Wedo(const char* name,void (*f)(int))
{
  setName(name);
  addBLEhandler(f);
}
int Wedo::connect(){
  //return recieved;
  if (btStart()) {
    gattc_client_test();
  }
  return 1;
}
int Wedo::writeOutputCommand(uint8_t* command,int size)
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
  writeBLECommand(WEDO_OUTPUT,command,size);
}
int Wedo::writeInputCommand(uint8_t input_command[],int size)
{
  /*
  Write an output command to the output characteristic of the WEDO
  */
  //recieved = false;
  //Make a safety buffer so there won't be any loss of messages due to sending two messages at the same time
  //It waits till the last message was sent with a maximum of 100ms
  /*for(int i=0;i<20&&!ready();i++){
    delay(5);
  }*/
  printf("writeInputCommand with len: %i \n",sizeof(input_command));
  writeBLECommand(WEDO_INPUT,input_command,size);
}
void Wedo::writeMotor(uint8_t wedo_port,int wedo_speed)
{
  //From http://www.ev3dev.org/docs/tutorials/controlling-wedo2-motor/
  //conversion from int (both pos and neg) to unsigned 8 bit int
  uint8_t speed_byte = wedo_speed;
  uint8_t command[] = {wedo_port,0x01,0x01,speed_byte};
  writeOutputCommand(command,sizeof(command));
}
void Wedo::writeIndexColor(uint8_t color){
    //From http://ofalcao.pt/blog/2016/wedo-2-0-colors-with-python
    uint8_t command[] = {0x06,0x04,0x01,color};
    writeOutputCommand(command,sizeof(command));
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
    writeOutputCommand(command,sizeof(command));
}
boolean Wedo::connected(){
  return getBLEConnected();
}
boolean Wedo::ready(){
  return getBLEReady();
}
void Wedo::setRGBMode(){
  writePortDefinition(0x06, 0x17, 0x01, 0x02);
}
void Wedo::setDetectSensor(uint8_t port){
  writePortDefinition(port, ID_DETECT_SENSOR,0,0);
}
void Wedo::writePortDefinition (uint8_t port, uint8_t type, uint8_t mode, uint8_t format){
  uint8_t command[] = {0x01, 0x02, port, type, mode, 0x01, 0x00, 0x00, 0x00, format, 0x01};
  printf("writePortDefinition with len: %i \n",sizeof(command));
  writeInputCommand(command,sizeof(command));
}
void Wedo::addNotificationHandler(void (*f)(int)){
  addBLEhandler(f);
}
/*
int Wedo::isReady(){

}*/
