/*
The MIT License (MIT)

Copyright (c) 2014 bbx10node@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "htu21dflib.h"

static const char I2CDEV[] = "/dev/i2c-1";  // raspberry pi
static const uint8_t I2CADDR = 0x40;        // htu21df i2c address

// dweet thing_name. Be sure to make up your own thing name! If you do not,
// your data will be mixed with everyone else's data using the same thing
// name.
static const char THING_NAME[] = "flippant-boot";

int main(int argc, char *argv[])
{
    int rc;     // return code
    int i2cfd;  // i2c file descriptor
    float temperature, humidity;
    char curlstr[256];

    i2cfd = i2c_open(I2CDEV);
    if (i2cfd < 0) {
        printf("i2c_open failed %d\n", i2cfd);
        return -1;
    }

    rc = htu21df_init(i2cfd, I2CADDR);
    if (rc < 0) {
        printf("i2c_init failed %d\n", rc);
        return -2;
    }

    while (1) {
        rc = htu21df_read_temperature(i2cfd, &temperature);
        if (rc < 0) {
            printf("i2c_read_temperature failed %d\n", rc);
            return -3;
        }

        rc = htu21df_read_humidity(i2cfd, &humidity);
        if (rc < 0) {
            printf("i2c_read_humidity failed %d\n", rc);
            return -4;
        }
        // Format the command to dweet the data
        rc = snprintf(curlstr, sizeof(curlstr),
                "curl \"https://dweet.io/dweet/for/%s?temperature=%.1f&humidity=%.1f\"",
                THING_NAME, temperature, humidity);
        if (rc < 0) {
            printf("snprintf failed %d\n", rc);
        }
        // Run the command to dweet the data
        rc = system(curlstr);
        if (rc != 0) {
            printf("system failed %d\n", rc);
        }
        // Wait 10 seconds
        sleep(10);
    }

    rc = i2c_close(i2cfd);
    if (rc < 0) {
        printf("i2c_close failed %d\n", rc);
        return -5;
    }

    return 0;
}
