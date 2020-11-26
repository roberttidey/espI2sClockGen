# espI2sClockGen
A clock and pulse generator using the I2S interface on an esp8266

## Features
- Clock generation from 2Hz to 20MHz
- Any frequency may be used
- Searches for best match of clock dividers and bit length from 160MHz base clock
- Typically better than 0.1% match for frequencies < 100KHz
- Mark space ratio selection
- Frequency matching tolerance may be relaxed to get better mark space handling
- Pulse train generation based on defintions in files
- Web based GUI allowing control from PC, phone, tablet
- Wifi Management to allow easy initial router set up
- OTA software update
- Uses a special I2s library (i2sTXcircular) giving flexible control

## Build and use
- Install i2sTXcircular library (included)
- Install BaseSupport library (https://github.com/roberttidey/BaseSupport)
- Needs WifiManager library
- Edit passwords in BaseConfig.h
- Compile and upload in Arduino environment
- Set up wifi network management by connecting to AP and browsing to 192.168.4.1
- upload basic set of files from data folder using STA ip/upload
- further uploads can then be done using ip/edit
- normal interface is at ip/
- Set TargetHz and duty cycle to find best match
- Advanced shows calculated parameters
- Tolerance may be increased to find a match with most bits that meets tolerance
- bitsPerWord may be entered to find match that uses this number per I2S word cycle
- A range may be put in to bitsPerWord to allow searching e.g. 24,31



