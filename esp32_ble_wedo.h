/*
  Morse.h - Library for flashing Morse code.
  Created by David A. Mellis, November 2, 2007.
  Released into the public domain.
*/
#ifndef Wedo_h
#define Wedo_h
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE



#include "Arduino.h"
#include "wedo_color_definitions.h"
#include "ble_functions.h"

#define ID_MOTOR 1
#define ID_TILT_SENSOR 34 //0x23
#define ID_DETECT_SENSOR 35 //0x24

class Wedo
{
  public:
    Wedo(const char*);
    int connect();
    boolean connected();
    boolean ready();
    int writeOutputCommand(uint8_t* command);
    int writeInputCommand(uint8_t* command);
    void writeMotor(uint8_t wedo_port,int wedo_speed);
    void writeIndexColor(uint8_t color);
    void writeSound(unsigned int frequency, unsigned int length);
    void setRGBMode();
    void setDetectSensor(uint8_t port);
    void writePortDefinition(uint8_t port, uint8_t type, uint8_t mode, uint8_t format);
};

#endif
