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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "htu21dflib.h"

static const int MAX_TEMP_CONVERSION    = 50;   // milliseconds
static const int MAX_HUMI_CONVERSION    = 16;   // ms
static const int MAX_RESET_DELAY        = 15;   // ms

static uint8_t HTU21DF_READTEMP_NH      = 0xF3; // NH = no hold
static uint8_t HTU21DF_READHUMI_NH      = 0xF5;
static uint8_t HTU21DF_WRITEREG         = 0xE6;
static uint8_t HTU21DF_READREG          = 0xE7;
static uint8_t HTU21DF_RESET            = 0xFE;

#define sleepms(ms)     usleep((ms)*1000)

static uint8_t I2Caddr;

static int calc_crc8(const uint8_t *buf, int len);

int i2c_open(const char *i2cdevname_caller)
{
    int i2cfd;      // i2c file descriptor
    FILE *boardrev; // get raspberry pi board revision
    const char *i2cdevname;

    i2cdevname = i2cdevname_caller;
    boardrev = fopen("/sys/module/bcm2708/parameters/boardrev", "r");
    if (boardrev) {
        char aLine[80];
        if (fgets(aLine, sizeof(aLine), boardrev)) {
            long board_revision;
            // Older board revisions use i2c-0, newer use i2c-1
            board_revision = strtol(aLine, NULL, 10);
            if ((board_revision == 2) || (board_revision == 3)) {
                i2cdevname = "/dev/i2c-0";
            }
            else {
                i2cdevname = "/dev/i2c-1";
            }
        }
        fclose(boardrev);
    }

    if ((i2cdevname == NULL) || (*i2cdevname == '\0')) return -1;

    if ((i2cfd = open(i2cdevname, O_RDWR)) < 0) {
        printf("%s:Failed to open the i2c bus %d/%d\n", __func__, i2cfd,
                errno);
        return i2cfd;
    }
    return i2cfd;
}

int i2c_close(int i2cfd)
{
    return close(i2cfd);
}

int htu21df_init(int i2cfd, uint8_t i2caddr)
{
    uint8_t buf[32];    // i2c messages
    int rc;             // return code
    struct i2c_rdwr_ioctl_data msgbuf;
    struct i2c_msg reset[1] = {
        {i2caddr, 0, 1, &HTU21DF_RESET},
    };
    struct i2c_msg read_user_reg[2] = {
        {i2caddr, 0, 1, &HTU21DF_READREG},
        {i2caddr, I2C_M_RD, 1, buf}
    };

    I2Caddr = i2caddr;
    msgbuf.nmsgs = 1;
    msgbuf.msgs = reset;
    rc = ioctl(i2cfd, I2C_RDWR, &msgbuf);
    if (rc < 0) {
        printf("%s:htu21df I2C_RDWR failed %d/%d\n", __func__, rc, errno);
        return rc;
    }
    sleepms(MAX_RESET_DELAY);

    msgbuf.nmsgs = 2;
    msgbuf.msgs = read_user_reg;
    rc = ioctl(i2cfd, I2C_RDWR, &msgbuf);
    if (rc < 0) {
        printf("%s:htu21df I2C_RDWR failed %d/%d\n", __func__, rc, errno);
        return rc;
    }

    if (buf[0] != 0x02) {
        printf("%s:htu21df did not reset\n", __func__);
        return -1;
    }
    return 0;
}

int htu21df_read_temperature(int i2cfd, float *temperature)
{
    uint8_t buf[32];    // i2c messages
    int rc;             // return code
    uint16_t rawtemp;   // raw temperature reading
    struct i2c_rdwr_ioctl_data msgbuf;
    struct i2c_msg read_temp[2] = {
        {I2Caddr, 0, 1, &HTU21DF_READTEMP_NH},
        {I2Caddr, I2C_M_RD, 3, buf}
    };
    struct i2c_msg read_temp3[1] = {
        {I2Caddr, I2C_M_RD, 3, buf}
    };

    msgbuf.nmsgs = 2;
    msgbuf.msgs = read_temp;
    rc = ioctl(i2cfd, I2C_RDWR, &msgbuf);
    if (rc < 0) {
        //printf("I2C_RDWR %d/%d\n", rc, errno);
        sleepms(MAX_TEMP_CONVERSION);
        msgbuf.nmsgs = 1;
        msgbuf.msgs = read_temp3;
        rc = ioctl(i2cfd, I2C_RDWR, &msgbuf);
        if (rc < 0) {
            printf("%s:I2C_RDWR %d/%d\n", __func__, rc, errno);
            return rc;
        }
    }
    //printf("READTEMP = 0x%x 0x%x 0x%x\n", buf[0], buf[1], buf[2]);
    if (calc_crc8(buf, 3) != 0) {
        printf("%s:Bad CRC\n", __func__);
        return -1;
    }
    // Remove low 2 bits because they are status
    rawtemp = ((buf[0] << 8) | buf[1]) & 0xFFFC;
    //printf("rawtemp %x\n", rawtemp);
    *temperature = ((rawtemp / 65536.0) * 175.72) - 46.85;
    return 0;
}

int htu21df_read_humidity(int i2cfd, float *humidity)
{
    uint8_t buf[32];
    uint16_t rawhumi;   // raw humidity
    int rc;             // return code
    struct i2c_rdwr_ioctl_data msgbuf;
    struct i2c_msg read_humi[2] = {
        {I2Caddr, 0, 1, &HTU21DF_READHUMI_NH},
        {I2Caddr, I2C_M_RD, 3, buf}
    };
    struct i2c_msg read_temp3[1] = {
        {I2Caddr, I2C_M_RD, 3, buf}
    };

    msgbuf.nmsgs = 2;
    msgbuf.msgs = read_humi;
    rc = ioctl(i2cfd, I2C_RDWR, &msgbuf);
    if (rc < 0) {
        //printf("I2C_RDWR %d/%d\n", rc, errno);
        sleepms(MAX_HUMI_CONVERSION);
        msgbuf.nmsgs = 1;
        msgbuf.msgs = read_temp3;
        rc = ioctl(i2cfd, I2C_RDWR, &msgbuf);
        if (rc < 0) {
            printf("%s:I2C_RDWR %d/%d\n", __func__, rc, errno);
            return rc;
        }
    }
    //printf("READHUM= 0x%x 0x%x 0x%x\n", buf[0], buf[1], buf[2]);
    if (calc_crc8(buf, 3) != 0) {
        printf("%s:Bad CRC\n", __func__);
        return -1;
    }
    // Remove low 2 bits because they are status
    rawhumi = ((buf[0] << 8) | buf[1]) & 0xFFFC;
    //printf("rawhumi %x\n", rawhumi);
    *humidity = ((rawhumi / 65536.0) * 125.0) - 6.0;
    return 0;
}

// buf = 3 bytes from the HTU21DF for temperature or humidity
//       2 data bytes and 1 crc8 byte
// len = number of bytes in buf but it must be 3.
// return value < 0 error
// return value = 0 CRC good
// return value > 0 CRC bad
static int calc_crc8(const uint8_t *buf, int len)
{
    uint32_t dataandcrc;
    // Generator polynomial: x**8 + x**5 + x**4 + 1 = 1001 1000 1
    const uint32_t poly = 0x98800000;
    int i;

    if (len != 3) return -1;
    if (buf == NULL) return -1;

    // Justify the data on the MSB side. Note the poly is also
    // justified the same way.
    dataandcrc = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8);
    for (i = 0; i < 24; i++) {
        if (dataandcrc & 0x80000000UL)
            dataandcrc ^= poly;
        dataandcrc <<= 1;
    }
    return (dataandcrc != 0);
}

#ifdef HTU21DFTEST

static const char I2CDEV[] = "/dev/i2c-1";  // raspberry pi
static const uint8_t I2CADDR = 0x40;        // htu21df i2c address

int main(int argc, char *argv[])
{
    int rc;     // return code
    int i2cfd;  // i2c file descriptor
    int i;
    float temperature, humidity;
    char *i2c_devname;

    i2c_devname = I2CDEV;
    if (argc > 1) {
        i2c_devname = argv[1];
    }

    printf("opening %s\n", i2c_devname);
    i2cfd = i2c_open(i2c_devname);
    if (i2cfd < 0) {
        printf("i2c_open(%s) failed %d\n", i2c_devname, i2cfd);
        return -1;
    }

    rc = htu21df_init(i2cfd, I2CADDR);
    if (rc < 0) {
        printf("i2c_init failed %d\n", rc);
        return -2;
    }

    for (i = 0; i < 100; i++) {
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

        printf("%.1f %.1f\n", temperature, humidity);
        sleep(1);
    }

    rc = i2c_close(i2cfd);
    if (rc < 0) {
        printf("i2c_close failed %d\n", rc);
        return -5;
    }
    return 0;
}
#endif
