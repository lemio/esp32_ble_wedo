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

// DEVICE_TYPE_ANY_HUB accepts any hub - WeDo 2.0, Powered Up, BOOST, train, Duplo - but
// never a Remote Control, so this doesn't accidentally connect to a nearby remote
// instead of an actual hub with a motor. If you have more than one hub nearby, you can
// specify a name too: PoweredUp hub("devicename", DEVICE_TYPE_POWERED_UP_HUB);
PoweredUp hub(nullptr, DEVICE_TYPE_ANY_HUB);

// Which LEGO_COLOR_* the hub's LED is currently showing. Cycles 1-9 only - 0 and 10
// look like the LED is off/washed out on most hubs.
int colorIndex = 1;

void setup() {
  hub.connect();
  hub.onButtonPressed([](){
    colorIndex = (colorIndex % 9) + 1;
  });
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
