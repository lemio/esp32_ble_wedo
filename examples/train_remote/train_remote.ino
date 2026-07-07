/*
A LEGO Powered Up Remote Control driving a train hub, plus a 5-pixel NeoPixel strip
(wired to GPIO35) that shows the current speed as a bar - lights 0 to 5 pixels, in
whichever LEGO colour the hub/remote are currently showing.
Needs the Adafruit NeoPixel library (Library Manager / adafruit/Adafruit NeoPixel).

Controls:
- Remote port A up/down: step the train's speed by 10%. Hold to keep stepping (repeats
  every 200ms, like a keyboard key repeating while held) - remoteButton()'s onPressed()
  repeatMs parameter does this for us, no manual timer needed.
- Remote port A stop (middle/red): stop the train immediately (speed back to 0). Also
  takes priority over up/down if somehow held at the same time - built into the library.
- The train hub's own button (the LEGO-logo button on the hub, not the remote): cycles
  through the built-in LEGO colours - the hub's LED and the remote's LED always show the
  same colour, so the remote acts as a little status light for the hub too.

Also: if a simple LEGO Power Functions light (not a smart Powered Up light - the older
"LPF2-LIGHT" accessory) is plugged into the hub, it's driven as a direction indicator:
full on while driving forward, and "full on" in a sense while driving backward too, since
this old light doesn't know direction - see writeLight()'s comment below for why that's
still what "-100" does here.
*/

#include <PoweredUp.h>
#include <Adafruit_NeoPixel.h>

#define NEOPIXEL_PIN 35
#define NEOPIXEL_COUNT 5
#define NEOPIXEL_BRIGHTNESS 60  // 0-255, kept low so the strip isn't blinding indoors
#define SPEED_STEP_PERCENT 10
#define REPEAT_INTERVAL_MS 200

// RGB for each LEGO_COLOR_* index (see wedo_color_definitions.h), so the NeoPixel gauge
// can mirror whatever colour the hub/remote are currently showing. Values come from a
// LEGO colour reference table, matched up by *name* to wedo_color_definitions.h's
// existing index/name pairing - PURPLE, CYAN, and LIGHTGREEN aren't in that reference
// table, so those three are still approximate.
struct RGB {
  uint8_t r, g, b;
};
const RGB legoColorRGB[11] = {
    {0, 0, 0},         // 0  BLACK
    {244, 50, 50},     // 1  PINK
    {100, 31, 118},    // 2  PURPLE (approximate - not in the reference table, using Magenta)
    {30, 90, 168},     // 3  BLUE
    {104, 195, 226},   // 4  CYAN (approximate - using Medium Azure)
    {100, 200, 80},    // 5  LIGHTGREEN (approximate - not in the reference table)
    {0, 133, 43},      // 6  GREEN
    {250, 200, 10},    // 7  YELLOW
    {224, 102, 10},    // 8  ORANGE
    {180, 0, 0},       // 9  RED
    {244, 244, 244},   // 10 WHITE
};

// DEVICE_TYPE_ANY_HUB accepts any hub - WeDo 2.0, Powered Up, BOOST, train, Duplo - but
// never a Remote Control, so this slot can't accidentally grab the physical remote
// before the remote's own (more specific) slot below claims it.
PoweredUp hub(nullptr, DEVICE_TYPE_ANY_HUB);
PoweredUp remote(nullptr, DEVICE_TYPE_POWERED_UP_REMOTE);
Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

int trainSpeed = 0;  // -100..100, in steps of SPEED_STEP_PERCENT
// Which LEGO_COLOR_* the hub+remote LEDs are currently showing. Cycles 1-9 only -
// LEGO_COLOR_BLACK (0) looks like the LED is off, and LEGO_COLOR_WHITE (10) isn't part
// of the WeDo hub's discrete colour palette (only 9 real colours + off), so both ends
// of the 0-10 range are skipped to keep hub and remote showing the same visible colour.
int colorIndex = 1;

void setTrainSpeed(int newSpeed) {
  if (newSpeed > 100) newSpeed = 100;
  if (newSpeed < -100) newSpeed = -100;
  trainSpeed = newSpeed;

  hub.writeMotor(trainSpeed);

  // Direction indicator light, if one's plugged in. writeLight() takes -100..100 (like
  // everything else in this library) and scales it to whatever range the real device
  // uses internally - here that's a simple on/off-ish light, so full-scale in either
  // direction is "on".
  if (trainSpeed > 0) {
    hub.writeLight(100);
  } else if (trainSpeed < 0) {
    hub.writeLight(-100);
  } else {
    hub.writeLight(0);
  }
}

void updateSpeedGauge() {
  int litCount = abs(trainSpeed) / (100 / NEOPIXEL_COUNT); // 0..5
  if (litCount > NEOPIXEL_COUNT) litCount = NEOPIXEL_COUNT;

  RGB c = legoColorRGB[colorIndex];
  uint32_t litColor = strip.Color(
      (c.r * NEOPIXEL_BRIGHTNESS) / 255,
      (c.g * NEOPIXEL_BRIGHTNESS) / 255,
      (c.b * NEOPIXEL_BRIGHTNESS) / 255);

  for (int i = 0; i < NEOPIXEL_COUNT; i++) {
    strip.setPixelColor(i, i < litCount ? litColor : 0);
  }
  strip.show();
}

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show();

  Serial.println("Connecting to hub...");
  hub.connect();
  Serial.println("Connecting to remote...");
  remote.connect();
  Serial.println("Connected!");

  RemoteButtonHandle& btn = remote.remoteButton('A');
  btn.up.onPressed([](){ setTrainSpeed(trainSpeed + SPEED_STEP_PERCENT); }, REPEAT_INTERVAL_MS);
  btn.down.onPressed([](){ setTrainSpeed(trainSpeed - SPEED_STEP_PERCENT); }, REPEAT_INTERVAL_MS);
  btn.stop.onPressed([](){ setTrainSpeed(0); });

  // The hub's own physical button - cycles the shared colour, kept in sync on both LEDs.
  hub.onButtonPressed([](){
    colorIndex = (colorIndex % 9) + 1; // cycle 1..9, skipping black (0) and white (10)
    hub.writeIndexColor(colorIndex);
    remote.writeIndexColor(colorIndex);
  });

  hub.writeIndexColor(colorIndex);
  remote.writeIndexColor(colorIndex);
  setTrainSpeed(0);
}

void loop() {
  hub.handleConnection();
  remote.handleConnection();
  updateSpeedGauge();
  delay(20);
}
