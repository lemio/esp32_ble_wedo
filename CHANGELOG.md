# Changelog

## v3.0.0

**A rewrite around a new `PoweredUp` class, plus a much simpler callback API.** This is a breaking change from earlier versions — see Upgrading below.

### Broader hardware support
Previously WeDo 2.0-only, this library now speaks both WeDo 2.0 and LEGO's LWP3 protocol, so the same `PoweredUp` class controls WeDo 2.0, Powered Up, BOOST, train, and Duplo hubs, plus the Powered Up Remote Control — the library detects which protocol a connected device uses and handles it transparently. You can connect to more than one device at once (e.g. a hub and a Remote Control together), one `PoweredUp` object per device.

### Simpler callbacks
Every input used to require decoding a raw `int8_t*` byte array by hand. That's gone:

```cpp
hub.onButtonPressed([](){ hub.writeMotor(100); });
hub.onButtonReleased([](){ hub.writeMotor(0); });
hub.onDistanceChanged([](int8_t distance){ ... });
hub.onTiltChanged([](int8_t x, int8_t y){ ... });
```

Callbacks are lambdas (`std::function`), so they can capture variables. For explicit port targeting or to ask what's actually plugged in, use `port()`:

```cpp
hub.port('A').onDistanceChanged([](int8_t distance){ ... });
if (hub.port('A') == IO_TYPE_MOTION_SENSOR) { ... }
```

A Remote Control's three buttons are now a handle with built-in edge detection, key-repeat, and Stop-button priority — no more hand-rolled `upHeld`/`downHeld` state machines in your sketch:

```cpp
RemoteButtonHandle& btn = remote.remoteButton('A');
btn.up.onPressed([](){ ... }, 200);   // repeats every 200ms while held
btn.stop.onPressed([](){ ... });      // takes priority over up/down while held
```

The old `monitorButton()`, `monitorHubButton()`, `monitorTiltSensor()`, `monitorDistance()` methods are fully removed in favor of the above. `monitorInput()` remains as an advanced escape hatch.

### Bug fixes
- WeDo 2.0 sensors now correctly re-subscribe after a reconnect (previously silently stopped reporting).
- Fixed a race condition where port-less sensor monitoring (`onDistanceChanged()` with no port) could permanently lock onto the wrong port if called before the device's attach event arrived.
- The distance/detect sensor now reports a consistent 0-10 range on both WeDo 2.0 and Powered Up/BOOST hubs (previously 0-100 on WeDo 2.0 only, an undocumented mismatch).

### Examples
All six examples were revisited, each now with its own section in the README (description + snippet):
- **button_motor** — drive a motor with the ESP32's own button.
- **sensor_motor** — motor + LED driven by a distance sensor, hub-agnostic.
- **train_hub** — standalone Powered Up/train hub demo, no remote needed.
- **train_remote** — Remote Control driving a train hub, with key-repeat and a NeoPixel speed gauge.
- **wifi_control** — rebuilt with a remote-control-styled web UI (grey +/− and red STOP buttons, big and centred, live pressed-state) served over WiFi, reachable at `http://esp32.local/` via mDNS.
- **analog_throttle** — a breadboard potentiometer as a physical train throttle.

### Upgrading
Replace `Wedo`/old callback-based monitor calls with `PoweredUp` and the new methods above — see the README's API Reference for the full method list. `library.properties`'s name changed to "Powered Up BLE library for ESP32" to reflect the broader scope.
