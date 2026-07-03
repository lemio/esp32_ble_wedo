# esp32_ble_wedo
A library to control LEGO wedo with the ESP32 through Bluetooth low energy

## The button_motor.ino example:


https://github.com/user-attachments/assets/92703554-2661-43a6-8563-1c84ce84e086

## analog_throttle.ino — a breadboard knob driving a real LEGO train

A potentiometer wired to the ESP32 acts as a physical throttle for a real LEGO train:
turn it one way to speed up forward, the other way to reverse, center to stop. Pressing
the hub's own button cycles its LED colour. A phone running the official LEGO app, or a
JavaScript library on a laptop, can't do the knob part at all - they have no GPIO/ADC to
read a physical control from. An ESP32 does, and this library turns that knob into a
motor command in about 10 lines of code:

```cpp
#include <PoweredUp.h>

#define POT_PIN 8  // potentiometer wiper - outer legs to 3.3V and GND

PoweredUp hub; // connects to any supported LEGO hub - WeDo 2.0, Powered Up, BOOST, train hub
int colorIndex = 1;

void hubButtonAction(int8_t* value, int size) {
  if (size < 1 || value[0] != 1) return;
  colorIndex = (colorIndex % 9) + 1;
  hub.writeIndexColor(colorIndex); // pressing the hub's own button cycles its LED
}

void setup() {
  hub.connect();
  hub.monitorHubButton(hubButtonAction);
}

void loop() {
  hub.handleConnection();
  int raw = analogRead(POT_PIN);
  hub.writeMotor(map(raw, 0, 4095, -100, 100)); // knob position -> motor speed/direction
  delay(20);
}
```

<!-- TODO: record and embed a demo video here, showing the breadboard + potentiometer alongside the LEGO train responding to it -->

See [`examples/analog_throttle`](examples/analog_throttle) for the full sketch and wiring notes.


## Version 3.0.0 - the `PoweredUp` class 🎉

This library was rewritten around a new `PoweredUp` class that replaces the old `Wedo`
class - **this is a breaking change**, not a drop-in upgrade. Make one `PoweredUp`
object per LEGO device you want to talk to (a hub, a Remote Control, ...) - you can have
more than one connected to the same ESP32 at the same time. The same methods
(`writeMotor()`, `writeIndexColor()`, `monitorDistance()`, ...) work the same way
whether you're talking to a WeDo 2.0 hub or a Powered Up / BOOST / train hub - the
library figures out which protocol it's using for you. It's still built on the modern
**NimBLE-Arduino** library for performance, stability, and low memory usage.

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
    lemio/esp32_ble_wedo @ ^3.0.0
```

## API Reference

### PoweredUp(const char\* name = nullptr, LegoDeviceType deviceType = DEVICE_TYPE_ANY)

Makes an object representing one LEGO device. Pass a `name` to match a specific
device's advertised name, a `deviceType` to match a specific kind of device (see below),
both, or neither to connect to the first supported LEGO device found.

```cpp
PoweredUp hub;                                             // any supported LEGO device
PoweredUp hub(nullptr, DEVICE_TYPE_ANY_HUB);                // any hub, but never a Remote Control
PoweredUp remote(nullptr, DEVICE_TYPE_POWERED_UP_REMOTE);   // specifically a Remote Control
PoweredUp hub("My Train", DEVICE_TYPE_POWERED_UP_HUB);      // by name and kind
```

`LegoDeviceType` values: `DEVICE_TYPE_ANY`, `DEVICE_TYPE_ANY_HUB`, `DEVICE_TYPE_WEDO_HUB`,
`DEVICE_TYPE_DUPLO_TRAIN`, `DEVICE_TYPE_BOOST_HUB`, `DEVICE_TYPE_POWERED_UP_HUB` (train
hub, city hub, etc.), `DEVICE_TYPE_POWERED_UP_REMOTE`.

### connect(uint32_t timeoutMs = 30000)

Starts looking for the device and blocks until it's connected (or `timeoutMs` has
passed). Safe to call on more than one `PoweredUp` object in a row in `setup()`.

### connected() / ready()

`connected()` is true once the BLE connection is up. `ready()` is true once the last
command you sent has finished.

### handleConnection()

Call this every `loop()` - keeps the connection alive and lets background work
(reconnecting, re-subscribing sensors, discovering what's plugged in) happen.

### Actuators

```cpp
void writeMotor(int speed);                 // drives whichever motor is plugged in
void writeMotor(int port, int speed);       // port: 'A'/'B' or 1/2, if you need to be specific
void writeIndexColor(uint8_t color);        // a built-in LEGO colour, 0-10 - see LEGO_COLOR_* below
void writeRGB(uint8_t red, uint8_t green, uint8_t blue); // mix your own colour, each channel 0-255
void writeSound(unsigned int frequency, unsigned int length); // WeDo hubs only - piezo speaker
void writeLight(int value);                 // a plugged-in simple LEGO Power Functions light, -100..100
```

`speed`/`value` ranges are all -100 to 100. `writeMotor(speed)`/`writeLight(value)` find
the right port themselves - you only need the port-specific overload if more than one
matching device is attached (e.g. two motors on one hub).

`writeIndexColor()`'s built-in colours:
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

### Sensors

Work on both WeDo 2.0 hubs and Powered Up / BOOST / train hubs:

```cpp
void monitorButton(inputHandlerFunction callback);                  // Remote Control buttons: value[0]=up, value[1]=stop, value[2]=down
void monitorButton(int port, inputHandlerFunction callback);
void monitorDistance(inputHandlerFunction callback);                 // a distance/proximity sensor
void monitorDistance(int port, inputHandlerFunction callback);
void monitorTiltSensor(inputHandlerFunction callback);                // tilt angle, x/y degrees, -45 to 45
void monitorTiltSensor(int port, inputHandlerFunction callback);
void monitorHubButton(inputHandlerFunction callback);                  // the hub's own physical button
void monitorInput(int port, inputHandlerFunction callback, uint8_t mode); // advanced: a specific mode yourself
void stopMonitoring(int port);   // stop listening on one port
void stopMonitoring();           // stop all monitoring on this connection
```

The no-port overloads find the right device themselves; on a WeDo 2.0 hub (which can't
report what's plugged in on its own) this falls back to assuming port A unless told
otherwise. `monitorInput()` is the general form the others are built on - see the
`*Mode` enums in `PoweredUp.h`, or the "Port mode information" table the library prints
to Serial whenever something attaches, for what modes a given device actually supports.

### Advanced / escape hatches

```cpp
void writeCommand(uint8_t* command, int size, int type = WEDO_OUTPUT); // send a raw command yourself
void writePortDefinition(uint8_t port, uint8_t type, uint8_t mode, uint8_t format); // WeDo 2.0 only
void addNotificationHandler(void (*f)(uint8_t*, int)); // replaces the library's own notification handling with your own
```

Most sketches won't need these - `writeCommand()`'s `type` defaults to `WEDO_OUTPUT`
(motor/LED/sound commands); `WEDO_INPUT` is for the handful of WeDo-specific commands
that need it.

## Examples

* analog_throttle.ino (a breadboard potentiometer as a physical throttle for a real LEGO train - see above).
* button_motor.ino (it let's you controll the motor with the build in button on the ESP). (Nice start if you want to make a remote for you WEDO creation)
* sensor_motor.ino (drives a motor's speed and an LED colour from a distance/detect sensor's reading).
* train_hub.ino (a simple standalone Powered Up / BOOST / train hub demo: motor, LED, tilt sensor).
* train_remote.ino (a Remote Control driving a train hub - speed ramp with key-repeat, synced LED colours, a NeoPixel speed gauge).
* wifi_control.ino (it let's you set the direction of the motor connected to the wedo, over a WiFi web page instead of BLE input).

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
