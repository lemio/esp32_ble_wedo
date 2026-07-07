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

const FIRMWARE_CONFIGS = {
    'amoled-t-display': {
        name: 'Amoled T-Display Factory Firmware',
        description: 'Default factory firmware for Amoled T-Display',
        expectedBehavior: [
            'Display shows factory test screen',
            'Touch interface should be responsive',
            'LEDs may light up in sequence'
        ],
        files: [
            { path: 'Amoled-T-Display/Factory/Firmware/partitions.bin', offset: 0x8000 },
            { path: 'Amoled-T-Display/Factory/Firmware/boot_app0.bin', offset: 0xe000 },
            { path: 'Amoled-T-Display/Factory/Firmware/firmware.bin', offset: 0x10000 }
        ]
    },
    'amoled-screencast': {
        name: 'Amoled T-Display Screencast',
        description: 'WebRTC streaming firmware with WiFi configuration',
        expectedBehavior: [
            'Device connects to WiFi network <b>|*S*|</b> with password <b>|*P*|</b>',
            'Control the display via <a href="/Amoled-T-Display/Screencast/webrtc_stream.html?espAddress=|*M*|.local&sourceType=screen" target="_blank">this interface</a>',
            'Access web local interface at <a href="http://|*M*|.local" target="_blank">http://|*M*|.local</a>',
            'Stream display content via WebRTC',
            'Display shows connection status'
        ],
        files: [
            { path: 'Amoled-T-Display/Screencast/Firmware/bootloader.bin', offset: 0x0000 },
            { path: 'Amoled-T-Display/Screencast/Firmware/partitions.bin', offset: 0x8000 },
            { path: 'Amoled-T-Display/Screencast/Firmware/boot_app0.bin', offset: 0xe000 },
            { path: 'Amoled-T-Display/Screencast/Firmware/firmware.bin', offset: 0x10000 }
        ],
        variables: [
            {   
                firmware_name: '|*S*|', 
                readable_name: 'WiFi Name (SSID)', 
                default_value: 'ESP32-S3-T-Display', 
                max_length: 100 
            },
            {   
                firmware_name: '|*P*|', 
                readable_name: 'WiFi Password', 
                default_value: 'testtest', 
                max_length: 100 
            },
            {   
                firmware_name: '|*M*|', 
                readable_name: 'MDNS Hostname', 
                default_value: 'esp32-s3-t-display', 
                postfix: '.local', 
                max_length: 100 
            },
            {   
                firmware_name: 'WebRTC Stream', 
                readable_name: 'Display Name', 
                default_value: 'WebRTC Stream', 
                max_length: 13 
            }
        ]
    }
};

// Export for ES6 modules
export { CONFIG, FIRMWARE_CONFIGS };
