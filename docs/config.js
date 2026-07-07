// ESP32-S3 Flash Configuration
// This file contains the configuration for both index.html and wizard.html

const CONFIG = {
    DISCONNECT_WAIT_MS: 1500,
    BAUD_RATE: 115200,
    CHIP_NAME: "ESP32-S3",
    FILTERS: [
        {usbVendorId: 0x10C4},    // SILICON_LABS
        {usbVendorId: 0x303A},    // ESPRESSIF
        {usbVendorId: 0x0403},    // FTDI
        {usbVendorId: 0x1B4F},    // SparkFun
        {usbVendorId: 0x2341}     // Arduino
    ]
    // Device and vendor IDs can be set in the esp32
    // https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb.html
    // Vendors IDs are protected by USB-IF and can be found online
    // https://devicehunt.com/view/type/usb/vendor/10C4/product/EA60
    //                              {usbProductId: 0xEA60,  usbVendorId: 0x10C4}
    // ESP32-S3-Devkit-C1 UART Port: {(CP210x UART Bridge),  (SILICON_LABS)}
    // ESP32-S3-Devkit-C1 USB Port: {usbProductId: 4097 0x1001, usbVendorId: 12346 = 0x303A (ESPRESSIF)}
};

// No firmwares are hardcoded here anymore - this file is a generic flashing UI that any
// repo can point at its own manifest.json (see README.md's "For PlatformIO projects"
// section), or you can hand-add entries here directly. Shape of an entry, for reference:
//
// const FIRMWARE_CONFIGS = {
//   'my-firmware-key': {
//     name: 'Human-readable name',
//     description: 'Brief description (wizard only)',
//     hardware: 'ESP32-S3-DevKitC-1',      // board/chip this firmware targets - shown in the UI
//     expectedBehavior: [                   // wizard only
//       'What happens after flashing',
//       'Can include HTML like <b>bold</b> or <a href="...">links</a>'
//     ],
//     files: [                              // firmware files to flash, standard ESP32 layout
//       { path: 'path/to/bootloader.bin', offset: 0x0000 },
//       { path: 'path/to/partitions.bin', offset: 0x8000 },
//       { path: 'path/to/boot_app0.bin',  offset: 0xe000 },
//       { path: 'path/to/firmware.bin',   offset: 0x10000 }
//     ],
//     variables: [                          // optional: flash-time-patchable placeholders
//       {
//         firmware_name: '|*S*|',           // exact string reserved in the compiled binary
//         readable_name: 'WiFi Name',       // label shown to the user
//         default_value: 'MyNetwork',       // shown as the default in the UI
//         max_length: 100,                  // must match the fixed-size array in firmware source
//         postfix: '.local'                 // optional: appended after the input in the UI
//       }
//     ]
//   }
// };
const FIRMWARE_CONFIGS = {};

// Export for ES6 modules
export { CONFIG, FIRMWARE_CONFIGS };
