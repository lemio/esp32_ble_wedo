## Getting output devices working
- motor
- LED
- piezo

## Getting sensor working
- notify system works (with the button on the wedo)
- putting the commands directly on the nRF connect works
  - info from https://github.com/vheun/wedo2/blob/master/index.js
  - register for notify to 1560
  - set 1563 to 0x01 02 02 23 00 01 00 00 00 00 01
  - you get data in the range 02-02-00 -> 02-02-0A
  - printing the exact sequence in the Serial monitor
    - Found error, only prints the first 5 uint8_t's of the array
    - Probably some pointer issue
    - Also explains the piezo frequency issues
    - Try to do some sizeof's in the program
    - Solved the issue by sending a size with every function that receieves an array
  || swopping input and output in the search for characteristic function (already solved the issue)
  - passing a callbackHandler to the BLE stack (for notifications)

- detect Sensor
