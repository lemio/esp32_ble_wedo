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
#define ID_TILT_SENSOR 34
//0x23
#define ID_DETECT_SENSOR 35
//0x24
#define RANGE_10 0
#define RANGE_100 1
#define RANGE_RAW 2
static void _WEDOnotificationHandler(uint8_t* data,int size);
#define WEDO_PORTS 2
static uint8_t devices[WEDO_PORTS] = {0,0};
typedef void (*inputHandlerFunction)(int8_t*,int);
static void (*portHandlers[WEDO_PORTS])(int8_t*,int);

//static void (*port2Handler)(uint8_t*,int);

class Wedo
{
  public:
    Wedo(const char*);//,void (*f)(int));
    int connect();
    boolean connected();
    boolean ready();
    int writeOutputCommand(uint8_t* command,int size);
    int writeInputCommand(uint8_t* command,int size);
    void writeMotor(uint8_t wedo_port,int wedo_speed);
    void writeIndexColor(uint8_t color);
    void writeSound(unsigned int frequency, unsigned int length);
    void writeRGB(uint8_t red, uint8_t green, uint8_t blue);
    void setRGBMode();
    void setIndexMode();
    void setTiltSensor(uint8_t port,inputHandlerFunction portHandler);
    void setDetectSensor(uint8_t port,inputHandlerFunction portHandler);
    void writePortDefinition(uint8_t port, uint8_t type, uint8_t mode, uint8_t format);
    void addNotificationHandler(void (*f)(uint8_t*,int));
};

#endif
