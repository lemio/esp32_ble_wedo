/*
  Morse.h - Library for flashing Morse code.
  Created by David A. Mellis, November 2, 2007.
  Released into the public domain.
*/
#ifndef Wedo_h
#define Wedo_h
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE



#include "Arduino.h"
#include "ble_functions.h"


class Wedo
{
  public:
    Wedo(const char*);
    int connect();
    boolean connected();
    boolean ready();
    int writeOutputCommand(uint8_t* command);
    void writeMotor(uint8_t wedo_port,int wedo_speed);
    void writeIndexColor(uint8_t color);
    void writeSound(unsigned int frequency, unsigned int length);
};

#endif
