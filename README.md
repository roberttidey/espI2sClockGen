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

## Build
- Install i2sTXcircular library (included)
- Install BaseSupport library (https://github.com/roberttidey/BaseSupport)
- Needs WifiManager library
- Edit passwords in BaseConfig.handling
- Compile and upload in Arduino environment



