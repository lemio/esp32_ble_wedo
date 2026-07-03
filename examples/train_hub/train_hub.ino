/*
A simple standalone example for a LEGO Powered Up train hub - no remote control needed.

Drives whichever motor is plugged in back and forth, and lights the hub's LED green
while going forward and red while going backward. If a tilt sensor is plugged into the
hub, its x/y angle is printed to Serial too.

This library also prints a "Port mode information" table to Serial automatically
whenever something is plugged in, listing every mode it supports - handy for figuring
out what's actually attached to your hub.
*/

#include <PoweredUp.h>

// Naming it specifically as a hub (rather than any supported LEGO device) means this
// sketch won't accidentally connect to a Remote Control if one happens to be nearby too.
PoweredUp hub(nullptr, DEVICE_TYPE_POWERED_UP_HUB);

void handleTilt(int8_t* value, int size) {
  if (size < 2) return;
  Serial.print("Tilt angle: x=");
  Serial.print(value[0]);
  Serial.print(" y=");
  Serial.println(value[1]);
}

void setup() {
  Serial.begin(115200);

  Serial.println("Connecting to hub...");
  hub.connect();
  Serial.println("Connected!");

  // Only does anything if a tilt sensor is actually plugged in - finds it itself,
  // wherever it is.
  hub.monitorTiltSensor(handleTilt);
}

void loop() {
  hub.handleConnection();

  hub.writeRGB(0, 255, 0); // green: driving forward
  hub.writeMotor(60);      // no port given - drives whichever motor is plugged in
  delay(2000);

  hub.writeRGB(255, 0, 0); // red: driving backward
  hub.writeMotor(-60);
  delay(2000);
}
