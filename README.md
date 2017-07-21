# esp32_ble_wedo
A library to control LEGO wedo with the ESP32 through Bluetooth low energy

### myWedo(char* name)

Connect to a WEDO2.0 that is advertising with the defined name.

### myWedo.connect()

Start connecting to the WEDO2.0 (do this after the wifi is initialized, if you're using wifi)

### myWedo.writeMotor(uint8_t wedo_port,int wedo_speed)

Writes a certain speed (-100,100) to the specified port.

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
