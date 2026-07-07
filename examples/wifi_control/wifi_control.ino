/*
  WiFi Web Remote - Motor Control

 Serves a page styled like the physical LEGO Powered Up Remote Control: a grey "+"
 button on top drives the motor forward, a red STOP button in the middle stops it, and
 a grey "-" button on the bottom drives it backward - big, centred, and highlighted
 while pressed (mouse or touch), same as pressing a real button.

 Also starts an mDNS responder, so the page is reachable at http://esp-poweredup.local/
 instead of having to look up its IP address from Serial.

 This example is written for a network using WPA encryption. For
 WEP or WPA, change the Wifi.begin() call accordingly.

 Circuit:
 * WiFi shield attached

 created for arduino 25 Nov 2012
 by Tom Igoe

ported for sparkfun esp32
31.01.2017 by Jan Hendrik Berlin

changed for a LEGO hub example by Geert Roumen

@Hardware
https://www.analoglamb.com/product/esp32-development-board/
@library
https://github.com/lemio/esp32_PoweredUp
@Status
Working
@licence
https://creativecommons.org/licenses/by-sa/4.0/

if you look in front of the hub's ports,
so the back of the hub, on port 1 there
is a LEGO motor connected
 _________________
|  port2 | port1  |
|________|________|
|                 |
|                 |
|_________________|

 */
#include <PoweredUp.h>

#include <WiFi.h>
#include <ESPmDNS.h>
// DEVICE_TYPE_ANY_HUB connects to any supported LEGO hub, but never a Remote Control -
// so this can't accidentally connect to a nearby remote instead of an actual hub.
PoweredUp hub(nullptr, DEVICE_TYPE_ANY_HUB);

// Fixed-size, null-padded strings - not meant to be hand-edited. A browser flashing
// tool (e.g. https://github.com/lemio/ESP32-S3-Flasher) can find these exact strings in
// the compiled binary and overwrite these 100-byte slots with real values at flash
// time, without needing to recompile. If you're building from source instead, just
// replace the whole string with your own value.
//
// WIFI_SSID/WIFI_PASSWORD have no sensible default (there's no "your WiFi" to guess),
// so they stay obvious placeholders - patch them via the flasher, or replace them
// yourself before building. MDNS_HOST's default is a real, usable hostname instead of a
// placeholder, so a board flashed straight from `pio run --target upload` (skipping the
// browser flasher entirely) is still reachable at http://esp-poweredup.local/ - the
// flasher can still search for and replace "esp-poweredup" itself if you want a
// different name for multiple boards on the same network.
const char WIFI_SSID[100]     = "|*S*|";
const char WIFI_PASSWORD[100] = "|*P*|";
const char MDNS_HOST[100]     = "esp-poweredup"; // reachable as http://esp-poweredup.local/

WiFiServer server(80);

// The remote-control page. Buttons fire their command with fetch() in the background
// (no page reload) as soon as they're pressed, and toggle a "pressed" CSS class between
// pointer-down and pointer-up so you can see which one is currently held - mouse and
// touch both handled, so it works from a phone too.
const char PAGE[] = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LEGO Remote</title>
<style>
  html, body {
    height: 100%;
    margin: 0;
    background: #1c1c1e;
    display: flex;
    align-items: center;
    justify-content: center;
    font-family: -apple-system, Helvetica, Arial, sans-serif;
    -webkit-tap-highlight-color: transparent;
    user-select: none;
  }
  .remote {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 28px;
  }
  button {
    width: 170px;
    height: 170px;
    border-radius: 50%;
    border: none;
    font-size: 68px;
    font-weight: bold;
    color: white;
    box-shadow: 0 8px 0 rgba(0, 0, 0, 0.4);
    transition: transform 0.05s, box-shadow 0.05s, background 0.05s;
  }
  .grey {
    background: #8a8d90;
  }
  .grey.pressed {
    background: #6e7174;
    transform: translateY(8px);
    box-shadow: 0 0 0 rgba(0, 0, 0, 0.4);
  }
  .stop {
    width: 200px;
    height: 200px;
    background: #c40000;
    font-size: 36px;
    letter-spacing: 2px;
  }
  .stop.pressed {
    background: #930000;
    transform: translateY(8px);
    box-shadow: 0 0 0 rgba(0, 0, 0, 0.4);
  }
</style>
</head>
<body>
<div class="remote">
  <button id="up" class="grey">+</button>
  <button id="stop" class="stop">STOP</button>
  <button id="down" class="grey">&minus;</button>
</div>
<script>
function wireButton(btn, url) {
  function press(e) {
    e.preventDefault();
    btn.classList.add('pressed');
    fetch(url);
  }
  function release() {
    btn.classList.remove('pressed');
  }
  btn.addEventListener('mousedown', press);
  btn.addEventListener('touchstart', press);
  btn.addEventListener('mouseup', release);
  btn.addEventListener('mouseleave', release);
  btn.addEventListener('touchend', release);
  btn.addEventListener('touchcancel', release);
}
wireButton(document.getElementById('up'), '/F');
wireButton(document.getElementById('stop'), '/S');
wireButton(document.getElementById('down'), '/B');
</script>
</body>
</html>
)HTML";

void setup()
{
    Serial.begin(115200);

    delay(10);

    // We start by connecting to a WiFi network

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Reachable as http://<MDNS_HOST>.local/ instead of the IP address above.
    if (MDNS.begin(MDNS_HOST)) {
      MDNS.addService("http", "tcp", 80);
      Serial.print("mDNS responder started - try http://");
      Serial.print(MDNS_HOST);
      Serial.println(".local/");
    }

    server.begin();

    hub.connect();
    while (!hub.connected()){
    Serial.print(".");
    delay(100);
    }

}

void loop(){
  // Handle automatic reconnection if connection is lost
  hub.handleConnection();

  WiFiClient client = server.available();   // listen for incoming clients
  if (!client) {
    return;
  }

  // Just the request line ("GET /F HTTP/1.1") is enough to route the request - the
  // remaining headers are read and discarded below.
  String requestLine = client.readStringUntil('\n');
  requestLine.trim();
  while (client.connected()) {
    String header = client.readStringUntil('\n');
    header.trim();
    if (header.length() == 0) {
      break; // blank line = end of the request headers
    }
  }

  if (requestLine.startsWith("GET /F")) {
    hub.writeMotor(1, 100);       // GET /F drives the motor forward
    client.println("HTTP/1.1 204 No Content");
    client.println("Connection: close");
    client.println();
  } else if (requestLine.startsWith("GET /S")) {
    hub.writeMotor(1, 0);         // GET /S stops the motor
    client.println("HTTP/1.1 204 No Content");
    client.println("Connection: close");
    client.println();
  } else if (requestLine.startsWith("GET /B")) {
    hub.writeMotor(1, -100);      // GET /B drives the motor backwards
    client.println("HTTP/1.1 204 No Content");
    client.println("Connection: close");
    client.println();
  } else {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();
    client.print(PAGE);
  }

  client.stop();
}
