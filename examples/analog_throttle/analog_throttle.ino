/*
A breadboard potentiometer as a physical throttle for a real LEGO train: turn it one
way, the train speeds up forward; the other way, it speeds up in reverse; centered, it
stops. Pressing the hub's own physical button cycles its LED through a few colours.

Wiring: potentiometer wiper -> POT_PIN, outer legs -> 3.3V and GND.

Works unmodified with a WeDo 2.0, Powered Up, BOOST, or train hub - PoweredUp connects
to whichever supported LEGO hub is nearby, and writeMotor() finds the motor itself, no
port number needed.
*/
#include <PoweredUp.h>

#define POT_PIN 8  //(1-10 for ADC1)

//This searches for any LEGO device nearby (remotes, hubs, lego wedo, etc.) and connects to the first one it finds. 
//If you have more than one LEGO device nearby, you can specify a name or device type in the constructor to connect to a specific one.
//PoweredUp hub("devicename", DEVICE_TYPE_POWERED_UP_HUB); //Connect to a specific hub by name
PoweredUp hub;

// Which LEGO_COLOR_* the hub's LED is currently showing. Cycles 1-9 only - 0 and 10
// look like the LED is off/washed out on most hubs.
int colorIndex = 1;

// The hub's own physical button - only react to it being pressed, not released, so one
// press = one colour change rather than two.
void hubButtonAction(int8_t* value, int size) {
  if (size < 1 || value[0] != 1) return;
  colorIndex = (colorIndex % 9) + 1;
}

void setup() {
  hub.connect();
  hub.monitorHubButton(hubButtonAction);
  hub.writeIndexColor(colorIndex);
}

void loop() {
  hub.handleConnection();

  int raw = analogRead(POT_PIN);            // 0-4095 on the ESP32's ADC
  int speed = map(raw, 0, 4095, -100, 100); // knob position -> motor speed/direction
  hub.writeMotor(speed); // no port given - auto-detects the motor. To target a specific port instead, use hub.writeMotor('A', speed) or hub.writeMotor('B', speed).
  //Index color is only written when it changes.
  hub.writeIndexColor(colorIndex);
  delay(20);
}
