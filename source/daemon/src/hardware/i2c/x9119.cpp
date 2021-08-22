#include "hardware/i2c/x9119.h"
#include <linux/i2c-dev.h> // I2C bus definitions for linux like systems
#include <linux/i2c.h> // I2C bus definitions for linux like systems
#include <stdint.h>
#include <stdio.h>

/*
* X9119
*/

unsigned int X9119::readWiperReg()
{

    // just return the locally stored last value written to WCR
    // since readback doesn't work without repeated start transaction
    return fWiperReg;

    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device
    uint8_t readBuf[16]; // 2 byte buffer to store the data read from the I2C device
    int16_t val; // Stores the 16 bit value of our ADC conversion

    writeBuf[0] = 0x80; // op-code read WCR

    // Write writeBuf to the X9119
    // this sets the write address for WCR register and writes WCR
    write(writeBuf, 1);

    readBuf[0] = 0;
    readBuf[1] = 0;

    read(readBuf, 2); // Read the config register into readBuf

    val = (readBuf[0] & 0x03) << 8 | readBuf[1];

    return val;
}

unsigned int X9119::readDataReg(uint8_t reg)
{

    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device
    uint8_t readBuf[16]; // 2 byte buffer to store the data read from the I2C device
    int16_t val; // Stores the 16 bit value of our ADC conversion

    writeBuf[0] = 0x80 | 0x20 | (reg & 0x03) << 2; // op-code read data reg

    // Write writeBuf to the X9119
    // this sets the write address for WCR register and writes WCR
    write(writeBuf, 1);

    readBuf[0] = 0;
    readBuf[1] = 0;

    read(readBuf, 2); // Read the config register into readBuf

    val = (readBuf[0] & 0x03) << 8 | readBuf[1];

    return val;
}

unsigned int X9119::readWiperReg2()
{
    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device
    uint8_t readBuf[16]; // 2 byte buffer to store the data read from the I2C device
    int16_t val; // Stores the 16 bit value of our ADC conversion

    writeBuf[0] = 0x80; // op-code read WCR

    struct i2c_msg rdwr_msgs[2];

    rdwr_msgs[0].addr = fAddress;
    rdwr_msgs[0].flags = 0;
    rdwr_msgs[0].len = 1;
    rdwr_msgs[0].buf = writeBuf;

    rdwr_msgs[1].addr = fAddress;
    rdwr_msgs[1].flags = I2C_M_RD;
    rdwr_msgs[1].len = 2;
    rdwr_msgs[1].buf = readBuf;

    struct i2c_rdwr_ioctl_data rdwr_data;

    rdwr_data.msgs = rdwr_msgs;
    rdwr_data.nmsgs = 2;

    int result = ioctl(fHandle, I2C_RDWR, &rdwr_data);

    if (result < 0) {
        printf("rdwr ioctl error: %d\n", errno);
        perror("reason");
    } else {
        printf("rdwr ioctl OK\n");
    }

    val = (readBuf[0] & 0x03) << 8 | readBuf[1];

    return val;

    writeBuf[0] = 0x80; // op-code read WCR

    // Write writeBuf to the X9119
    // this sets the write address for WCR register and writes WCR
    write(writeBuf, 1);

    readBuf[0] = 0;
    readBuf[1] = 0;

    read(readBuf, 2); // Read the config register into readBuf

    val = (readBuf[0] & 0x03) << 8 | readBuf[1];

    return val;
}

unsigned int X9119::readWiperReg3()
{

    union i2c_smbus_data data;
    struct i2c_smbus_ioctl_data args;

    args.read_write = I2C_SMBUS_READ;
    args.command = 0x80;
    args.size = I2C_SMBUS_WORD_DATA;
    args.data = &data;
    int result = ioctl(fHandle, I2C_SMBUS, &args);
    if (result < 0) {
        printf("rdwr ioctl error: %d\n", errno);
        perror("reason");
        return 0;
    } else {
        printf("rdwr ioctl OK\n");
    }
    return 0x0FFFF & data.word;
}

void X9119::writeWiperReg(unsigned int value)
{
    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device

    writeBuf[0] = 0x80 | 0x20; // op-code write WCR
    writeBuf[1] = ((value & 0xff00) >> 8); // MSB of WCR
    writeBuf[2] = (value & 0xff); // LSB of WCR

    // Write writeBuf to the X9119
    // this sets the write address for WCR register and writes WCR
    int n = write(writeBuf, 3);
    if (n == 3)
        fWiperReg = value;
}

void X9119::writeDataReg(uint8_t reg, unsigned int value)
{
    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device

    writeBuf[0] = 0x80 | 0x40 | (reg & 0x03) << 2; // op-code write data reg
    writeBuf[1] = ((value & 0xff00) >> 8); // MSB of WCR
    writeBuf[2] = (value & 0xff); // LSB of WCR

    // Write writeBuf to the X9119
    // this sets the write address for data register and writes dr with value
    write(writeBuf, 3);
}
