htu21dflib
==========

htu21df temperature and humidity sensor support for Raspberry Pi.

Adafruit HTU21D-F breakout board details.

http://www.adafruit.com/products/1899

Adafruit guide to configuring the Pi for i2c.

https://learn.adafruit.com/using-the-bmp085-with-raspberry-pi/configuring-the-pi-for-i2c

## Build it

```sh
./build.sh
```
## Run test program

```sh
./htu21dflib
```

Sample output

```sh
29.1 52.3
29.1 52.3
29.1 52.3
29.1 52.3
```

## Dweet your data

```sh
./dweetio
```

Login into http://freeboard.io to access your dweet data. Create a real-time
instrumentation panel with temperature and humidity guages without programming.

## Intel Edison

The HTU21D works with the Intel Edison Arduino breakout board. This does not
use the Arduino SDK or the IoT SDK. This program directly accesses the I2C
controller to control the sensor.

edison-i2c.sh enables the I2C controller on /dev/i2c-6. Run this only once.

htu21dflib.c takes the device name as an optional command line parameter.

```sh
./edison-i2c.sh
./htu21dflib /dev/i2c-6
```
