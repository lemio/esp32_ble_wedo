# esp32_ble_wedo
A library to control LEGO wedo with the ESP32 through Bluetooth low energy

The button_motor.ino example:


https://github.com/user-attachments/assets/92703554-2661-43a6-8563-1c84ce84e086


## Version 2.0.0 - Now using NimBLE-Arduino! 🎉

This library has been completely refactored to use the modern **NimBLE-Arduino** library for improved performance, stability, and reduced memory usage. The public API remains 100% compatible with previous versions.

### Requirements

- **ESP32** board
- **NimBLE-Arduino** library (automatically installed via Arduino Library Manager or add to platformio.ini)

### Installation

#### Arduino IDE
1. Install NimBLE-Arduino from Library Manager (search for "NimBLE-Arduino" by h2zero)
2. Install this library from Library Manager or download from GitHub

#### PlatformIO
Add to your `platformio.ini`:
```ini
lib_deps = 
    h2zero/NimBLE-Arduino @ ^1.4.0
    lemio/esp32_ble_wedo @ ^2.0.0
```

## API Reference

### myWedo(char* name = nullptr)

Connect to a matching LEGO hub by advertised name. Pass no name to connect to the first supported hub found.

Supported connection targets:
- WEDO 2.0 hubs
- Potentially LEGO Wireless Protocol 3.x hubs such as Powered Up / BOOST / train hubs

### myWedo.connect()

Start connecting to the supported LEGO hub (do this after the wifi is initialized, if you're using wifi)

### myWedo.writeMotor(uint8_t wedo_port,int wedo_speed)

Writes a certain speed (-100,100) to the specified port.

On WEDO hubs the ports are numbered `1` and `2`.
On Powered Up style hubs this library maps `1` to external port A and `2` to external port B.

If you look in front of the WEDO ports;
the back of the wedo, this is the port
overview
<pre>
 _________________
|  port2 | port1  |
|________|________|
|                 |
|                 |
|_________________|
</pre>

### myWedo.writeIndexColor(uint8_t color)

Sets the color of the RGB led on the wedo, you can choose from the list below
<pre>
#define LEGO_COLOR_BLACK 0
#define LEGO_COLOR_PINK 1
#define LEGO_COLOR_PURPLE 2
#define LEGO_COLOR_BLUE 3
#define LEGO_COLOR_CYAN 4
#define LEGO_COLOR_LIGHTGREEN 5
#define LEGO_COLOR_GREEN 6
#define LEGO_COLOR_YELLOW 7
#define LEGO_COLOR_ORANGE 8
#define LEGO_COLOR_RED 9
#define LEGO_COLOR_WHITE 10
</pre>
### myWedo.writeSound(unsigned int frequency, unsigned int length)

Let's the piezo in the WEDO make some noise, I'm not sure if the freqency and length are set correctly

Note: sound and the WEDO sensor configuration helpers are still WEDO-specific. Motor output and hub LED color are the parts currently adapted for LEGO Hub 3.x devices.

### myWedo.writeOutputCommand(uint8_t* command)

Sends a direct output command to the WEDO2.0

## Examples

* wifi_control.ino (it let's you set the direction of the motor connected to the wedo).
* button_motor.ino (it let's you controll the motor with the build in button on the ESP). (Nice start if you want to make a remote for you WEDO creation)

## Prior art

* [Official WEDO2.0 SDK from LEGO ](https://education.lego.com/en-us/support/wedo-2/developer-kits)
* [Nodejs implementation for Wedo2.0](https://github.com/vheun/wedo2)
* [Controlling a motor in linux with USB dongle](http://www.ev3dev.org/docs/tutorials/controlling-wedo2-motor/)
* [Sniffing data from WEDO2.0](http://ofalcao.pt/blog/2016/wedo-2-0-reverse-engineering)
* [Controlling a motor ](http://ofalcao.pt/blog/2016/controlling-wedo-2-0-motor-from-linux)
* [Controlling RGB LED in python](http://ofalcao.pt/blog/2016/wedo-2-0-colors-with-python)
* [Making apps for WEDO2.0 in app-inventor](http://ofalcao.pt/blog/2016/lego-wedo-2-0-with-mit-app-inventor)
* [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) - The BLE library powering this project
* [nRF Connect App](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile)
