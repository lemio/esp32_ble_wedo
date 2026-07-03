/*
  PoweredUp.h - Control LEGO WeDo 2.0 and LEGO Powered Up (train hubs, BOOST, Remote
  Control) over Bluetooth from an ESP32.

  Make one PoweredUp object per LEGO device you want to talk to (a hub, a remote
  control, ...) - you can have more than one connected at the same time.
*/
#ifndef PoweredUp_h
#define PoweredUp_h

#include "Arduino.h"
#include "wedo_color_definitions.h"
#include "ble_functions.h"

// --- WeDo 2.0 protocol constants -------------------------------------------------------
#define ID_MOTOR 1
#define ID_TILT_SENSOR 34   // 0x22
#define ID_DETECT_SENSOR 35 // 0x23
#define RANGE_10 0
#define RANGE_100 1
#define RANGE_RAW 2

// --- Which kind of LEGO BLE device to look for (PoweredUp's second constructor argument) ---
// Matches the "System Type + Device Number" byte LEGO devices advertise, so you can ask
// for e.g. "a Remote Control" specifically instead of "any supported LEGO device",
// which matters once more than one kind of LEGO device is turned on nearby.
enum LegoDeviceType : uint8_t {
  DEVICE_TYPE_ANY               = 0xFF, // no filtering - connect to the first supported device found
  DEVICE_TYPE_WEDO_HUB          = 0x00,
  DEVICE_TYPE_DUPLO_TRAIN       = 0x20,
  DEVICE_TYPE_BOOST_HUB         = 0x40,
  DEVICE_TYPE_POWERED_UP_HUB    = 0x41, // train hub, city hub, etc.
  DEVICE_TYPE_POWERED_UP_REMOTE = 0x42,
};

// --- IO Type IDs: what a hub reports when you plug something into a port ----------------
// Printed to Serial automatically whenever a device attaches, so you can always see
// these values for yourself rather than trusting this list blindly.
//
// The IO_TYPE_MEDIUM_MOTOR..IO_TYPE_LIGHT2 entries are older, "passive" LEGO Power
// Functions-style devices (the hub identifies them by a resistor/voltage pattern on
// their two ID pins rather than the device announcing itself) - reported as "LPF2-..."
// names by the discovery feature. All verified live; all are single-mode (mode 0) and
// output-only (actuators) except IO_TYPE_TOUCH, which is input-only.
enum LegoIoType : uint16_t {
  IO_TYPE_TILT_SENSOR   = 0x0022, // verified live: train hub port A/B
  IO_TYPE_MOTION_SENSOR = 0x0023, // verified live: train hub port A/B ("detect sensor")
  IO_TYPE_VOLTAGE       = 0x0014, // verified live: Remote Control internal port
  IO_TYPE_CURRENT       = 0x0015, // verified live: train hub internal port ("CUR L"/"CUR S")
  IO_TYPE_RSSI          = 0x0038, // verified live: Remote Control internal port
  IO_TYPE_REMOTE_BUTTON = 0x0037, // verified live: Remote Control port A/B
  IO_TYPE_RGB_LIGHT     = 0x0017, // verified live: hub LED, port varies by hub model

  IO_TYPE_MEDIUM_MOTOR  = 0x0001, // "LPF2-MMOTOR", -100..100
  IO_TYPE_TRAIN_MOTOR   = 0x0002, // "LPF2-TRAIN", -100..100 - the classic LEGO train motor
  IO_TYPE_TURN          = 0x0003, // "LPF2-TURN", -100..100
  IO_TYPE_POWER         = 0x0004, // "LPF2-POWER", -100..100
  IO_TYPE_TOUCH         = 0x0005, // "LPF2-TOUCH", input-only, 0 or 1 (pressed/not)
  IO_TYPE_LARGE_MOTOR   = 0x0006, // "LPF2-LMOTOR", -100..100
  IO_TYPE_XMOTOR        = 0x0007, // "LPF2-XMOTOR", -100..100
  IO_TYPE_LIGHT         = 0x0008, // "LPF2-LIGHT", -10..10
  IO_TYPE_LIGHT2        = 0x000a, // "LPF2-LIGHT2", 0..1
};

// --- Mode numbers, per device type ------------------------------------------------------
// A "mode" picks which measurement/format you get from a sensor, or which format you
// send to an actuator. monitorButton()/monitorDistance() already pick a sensible mode
// for you - these are for monitorInput(), when you want a specific one.
//
// The tilt/motion/current tables below come from running this library's own port/mode
// discovery feature (the table it prints to Serial when something attaches) against
// real hardware - not guessed from documentation.

// Hub/Remote LED (IO_TYPE_RGB_LIGHT) - verified live.
enum LedMode : uint8_t {
  LED_MODE_COL = 0, // a LEGO_COLOR_* index, 0-10 (used by writeIndexColor())
  LED_MODE_RGB = 1, // 3 raw bytes 0-255 (used by writeRGB())
};

// Remote Control button (IO_TYPE_REMOTE_BUTTON) - mode shapes/ranges verified live.
enum RemoteButtonMode : uint8_t {
  RCKEY = 0, // 1 byte: up(+)=1, down(-)=-1 (0xff), stop(red)=127, released=0 - USED BY monitorButton()... see KEYSD below
  KEYA  = 1, // 1 byte - exact meaning not yet verified
  KEYR  = 2, // 1 byte - exact meaning not yet verified
  KEYD  = 3, // 1 byte, range 0-7 - exact meaning not yet verified
  KEYSD = 4, // 3 separate bytes, each 0 or 1 - shape verified, USED BY monitorButton()
};

// Voltage sensor (IO_TYPE_VOLTAGE) - both modes' ranges verified live. "L"/"S" per the
// device's own reported names - likely "long"/"short" averaging windows.
enum VoltageMode : uint8_t {
  VLT_L = 0, // 0-9600 mV
  VLT_S = 1, // 0-9600 mV
};

// Current sensor (IO_TYPE_CURRENT) - verified live (name/mode shape), IO Type ID itself
// not independently confirmed - see IO_TYPE_CURRENT above.
enum CurrentMode : uint8_t {
  CUR_L = 0, // 0-2444 mA
  CUR_S = 1, // 0-2444 mA
};

// Signal strength (IO_TYPE_RSSI) - verified live, single mode, -80 to -30 dBm.
enum RssiMode : uint8_t {
  RSSI_MODE = 0,
};

// Motion/"detect" sensor (IO_TYPE_MOTION_SENSOR) - verified live. Mode 0 (DETECT) is
// what monitorDistance() uses.
enum MotionSensorMode : uint8_t {
  DETECT_MODE = 0, // 0-10, matches the WeDo-era "detect sensor" reading
  COUNT_MODE  = 1, // 0-100, 32-bit counter
  MOTION_CAL_MODE = 2, // 0-1023 raw, 3 datasets
};

// Tilt sensor (IO_TYPE_TILT_SENSOR) - verified live, all 4 modes. ANGLE (mode 0) is
// what monitorTiltSensor() uses, confirmed to be degrees, not an arbitrary unit.
enum TiltSensorMode : uint8_t {
  ANGLE_MODE = 0, // 2 datasets, -45 to 45 degrees (x/y tilt angle) - used by monitorTiltSensor()
  TILT_MODE  = 1, // 1 dataset, 0-10 (a coarser "which way is it tilted" direction reading)
  CRASH_MODE = 2, // 3 datasets, 0-100 (bump/impact counters per axis)
  TILT_CAL_MODE = 3, // 3 datasets, -45 to 45 (calibration values)
};

typedef void (*inputHandlerFunction)(int8_t*, int);

class PoweredUp {
  public:
    // Connect to a LEGO device by name, by kind (see LegoDeviceType), or both. Leave
    // both blank to connect to the first supported LEGO device found.
    PoweredUp(const char* name = nullptr, LegoDeviceType deviceType = DEVICE_TYPE_ANY);

    // Starts looking for the device and waits until it's connected (or timeoutMs has
    // passed). Safe to call on more than one PoweredUp object in a row in setup().
    int connect(uint32_t timeoutMs = 30000);
    boolean connected();
    boolean ready(); // true once the last command you sent has finished

    // Call this every loop() - keeps the connection alive and lets background work
    // (reconnecting, re-subscribing sensors, discovering what's plugged in) happen.
    void handleConnection();

    // Advanced: send a raw command yourself. type is WEDO_OUTPUT (the default - most
    // commands) or WEDO_INPUT (a few WeDo-specific ones). Most people won't need this.
    void writeCommand(uint8_t* command, int size, int type = WEDO_OUTPUT);

    // --- Actuators ---
    // Drives whichever motor is plugged in - if more than one is attached, or none has
    // been seen yet, this guesses port A. Use the two-argument version below if you need
    // to be specific (e.g. two motors on one hub).
    void writeMotor(int speed);
    // port: 'A'/'B' or 1/2 - which port the motor is plugged into.
    void writeMotor(int port, int speed);
    // A built-in LEGO colour, 0-10 (see the LEGO_COLOR_* constants).
    void writeIndexColor(uint8_t color);
    // Mix your own colour: red/green/blue, each 0 (none) to 255 (full).
    void writeRGB(uint8_t red, uint8_t green, uint8_t blue);
    // WeDo hubs only - plays a tone on the hub's built-in piezo speaker.
    void writeSound(unsigned int frequency, unsigned int length);
    // Drives a plugged-in simple LEGO Power Functions light (IO_TYPE_LIGHT), wherever
    // it's plugged in: value is -100 (off/dim) to 100 (full brightness).
    void writeLight(int value);

    // --- WeDo 2.0 low-level (WeDo hubs only) ---
    void writePortDefinition(uint8_t port, uint8_t type, uint8_t mode, uint8_t format);

    // --- Sensors (WeDo 2.0 hubs AND Powered Up / BOOST / train hubs) ---
    // Simple: listen for a Remote Control's buttons, reported as 3 bytes you can read
    // as booleans: value[0]=up, value[1]=stop, value[2]=down. Finds the button itself if
    // no port is given.
    void monitorButton(inputHandlerFunction callback);
    void monitorButton(int port, inputHandlerFunction callback);
    // Listen for a distance/proximity sensor. Finds it itself if no port is given; on a
    // WeDo 2.0 hub (which can't detect what's plugged in) this assumes port A unless told
    // otherwise.
    void monitorDistance(inputHandlerFunction callback);
    void monitorDistance(int port, inputHandlerFunction callback);
    // Listen for a tilt sensor's angle (x/y degrees, -45 to 45). Same port rules as
    // monitorDistance() above.
    void monitorTiltSensor(inputHandlerFunction callback);
    void monitorTiltSensor(int port, inputHandlerFunction callback);
    // Listen for the hub's own physical button (the LEGO-logo button on the hub itself -
    // not a Remote Control's buttons). Powered Up / BOOST / train hubs only.
    void monitorHubButton(inputHandlerFunction callback);
    // Advanced: listen to a specific mode yourself - see the *Mode enums above, or the
    // "Port mode information" table this library prints to Serial for what's actually
    // plugged in. Most people should use monitorButton()/monitorDistance()/
    // monitorTiltSensor() instead. Powered Up / BOOST / train hubs only.
    void monitorInput(int port, inputHandlerFunction callback, uint8_t mode);

    // Stops listening to whatever's on this port (from any of the monitor* calls above).
    void stopMonitoring(int port);
    // Stops all monitoring on this connection at once (every port, plus the hub button).
    void stopMonitoring();

    // Escape hatch: replaces the library's own notification handling with your own.
    void addNotificationHandler(void (*f)(uint8_t*, int));

  private:
    static const uint8_t MAX_SUBSCRIPTIONS = 8;
    static const uint8_t MAX_ATTACHED_DEVICES = 8;
    static const uint8_t MAX_PENDING_MONITORS = 4;
    static const uint8_t MAX_DISCOVERY_QUEUE = 8;
    static const uint8_t MAX_CANDIDATE_TYPES = 4;

    // One port this object has subscribed to input from, via monitorInput() (or
    // monitorButton()/monitorDistance(), which both call it).
    struct PortSubscription {
      bool inUse = false;
      uint8_t port = 0;
      uint8_t mode = 0;
      inputHandlerFunction handler = nullptr;
      bool reArmPending = false; // re-subscribe needed - the hub drops it whenever a port's device detaches
    };

    // Every port we've seen something attach to, and what it was (IO Type ID) - lets
    // monitorDistance()/monitorButton() find the right port themselves.
    struct AttachedDeviceInfo {
      bool inUse = false;
      uint8_t port = 0;
      uint16_t ioTypeId = 0;
    };

    // A monitorButton()/monitorDistance() call waiting for a matching device to attach
    // (either no port was given, or the given port didn't have a match yet).
    struct PendingMonitor {
      bool waiting = false;
      bool portGiven = false;
      uint8_t requestedPort = 0;
      uint8_t mode = 0;
      inputHandlerFunction callback = nullptr;
      uint16_t candidateTypes[MAX_CANDIDATE_TYPES] = {0, 0, 0, 0};
      uint8_t candidateCount = 0;
      const char* label = nullptr; // for the Serial message, e.g. "monitorDistance"
    };

    BLESlot _slot = BLE_SLOT_INVALID;

    // Set by addNotificationHandler() to bypass all of the library's own dispatch below.
    void (*_userNotificationOverride)(uint8_t*, int) = nullptr;

    // WeDo 2.0 per-port sensor config (WeDo hubs only ever have 2 external ports).
    uint8_t _wedoDevices[2] = {0, 0};
    inputHandlerFunction _wedoHandlers[2] = {nullptr, nullptr};

    // What's actually plugged into each WeDo 2.0 port, from the port-type
    // characteristic's attach/detach events (0 = nothing attached/unknown yet). Uses
    // the same device-id numbering as ID_TILT_SENSOR/ID_DETECT_SENSOR/IO_TYPE_LIGHT
    // etc. Kept separate from the LWP3 attached-device directory below since WeDo 2.0's
    // notification format and re-subscribe behaviour are different enough that mixing
    // them caused more confusion than it saved.
    uint8_t _wedoAttachedDevice[2] = {0, 0};

    PortSubscription _subscriptions[MAX_SUBSCRIPTIONS];
    AttachedDeviceInfo _attached[MAX_ATTACHED_DEVICES];
    PendingMonitor _pending[MAX_PENDING_MONITORS];

    // The hub's own physical button (monitorHubButton()) isn't tied to a port at all -
    // it's a separate LWP3 message type (Hub Properties), so it gets its own handler and
    // its own "resubscribe after reconnect" tracking instead of using the port machinery.
    inputHandlerFunction _hubButtonHandler = nullptr;
    bool _wasConnectedForHubButton = false;

    // Hub LED state (LWP3). _ledPort defaults to the train/Powered Up hub's LED port,
    // but a Remote Control's LED lives on a different port number - _ledModePort tracks
    // which port _ledActiveMode was actually confirmed on, so if _ledPort changes (e.g.
    // the real port is only known once its attach event arrives, which can race with an
    // early writeIndexColor()/writeRGB() call right after connect()) the mode gets
    // resent on the new port instead of assuming it's already set there.
    uint8_t _ledPort = 0x32;
    int8_t _ledActiveMode = -1;
    int _ledModePort = -1;
    int16_t _lastRGB[3] = {-1, -1, -1};
    int16_t _lastIndexColor = -1;
    bool _wasConnected = false;

    // Hub LED mode cache (WeDo 2.0) - same idea as _ledActiveMode above, so writeRGB()/
    // writeIndexColor() can switch modes for you without a separate setRGBMode() call.
    int8_t _wedoLedModeActive = -1;

    // Port/Port Mode discovery state machine - see PoweredUp.cpp for how this works.
    uint8_t _discoveryQueue[MAX_DISCOVERY_QUEUE];
    uint16_t _discoveryQueueIoType[MAX_DISCOVERY_QUEUE];
    uint8_t _discoveryQueueCount = 0;
    uint8_t _discoveryStep = 0;
    uint8_t _discoveryPort = 0;
    uint16_t _discoveryIoTypeId = 0; // IO Type ID of the port currently being discovered, printed alongside its modes so results can be matched back to a device later
    uint8_t _discoveryTotalModes = 0;
    uint16_t _discoveryInputModes = 0;
    uint16_t _discoveryOutputModes = 0;
    uint8_t _discoveryMode = 0;
    bool _discoveryStepComplete = false;
    unsigned long _discoveryStepSentAt = 0;
    char _modeName[12];
    float _modeRawMin, _modeRawMax;
    float _modePctMin, _modePctMax;
    float _modeSiMin, _modeSiMax;
    char _modeSymbol[6];
    uint8_t _modeNumDatasets, _modeDatasetType, _modeFigures, _modeDecimals;
    bool _haveName, _haveRaw, _havePct, _haveSi, _haveSymbol, _haveValueFormat;

    static void _notifyTrampoline(void* context, uint8_t* data, int size, BLENotificationSource source);
    void _handleNotification(uint8_t* data, int size, BLENotificationSource source);
    void _handleLwp3Notification(uint8_t* data, int size);
    void _handleWedoNotification(uint8_t* data, int size);
    void _handleWedoPortTypeNotification(uint8_t* data, int size);

    uint8_t _normalizePort(int portArg);

    int _findSubscription(uint8_t port);
    int _allocSubscription(uint8_t port);
    uint8_t _findMotorPort();
    void _recordAttached(uint8_t port, uint16_t ioTypeId);
    int _findAttachedPort(const uint16_t* candidateTypes, uint8_t candidateCount);

    void _monitorWithFallback(int portArg, bool portGiven, const uint16_t* candidateTypes,
                               uint8_t candidateCount, const char* label, uint8_t mode,
                               inputHandlerFunction callback);
    void _monitorWedoDevice(int portArg, bool portGiven, uint8_t deviceId, const char* label,
                             inputHandlerFunction callback);
    void _resolvePendingMonitors(uint8_t port, uint16_t ioTypeId);

    void _writeMotorRaw(uint8_t rawPort, int speed);
    void _writeLwpCommand(uint8_t port, uint8_t mode, const uint8_t* payload, uint8_t payloadSize);
    void _sendPortInputFormatSetup(uint8_t port, uint8_t mode, bool enabled = true);
    void _sendHubButtonSubscribe(bool enabled);
    void _ensureLwp3LedMode(uint8_t mode);
    void _ensureWedoLedMode(uint8_t mode);
    void _resetLedCacheOnReconnect();

    void _sendPortInformationRequest(uint8_t port, uint8_t infoType);
    void _sendPortModeInformationRequest(uint8_t port, uint8_t mode, uint8_t infoType);
    void _queuePortDiscovery(uint8_t port, uint16_t ioTypeId);
    void _startModeQuery();
    void _advanceDiscovery();
    void _printModeRow();
};

#endif
