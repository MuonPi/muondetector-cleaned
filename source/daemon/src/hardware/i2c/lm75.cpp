#include "hardware/i2c/lm75.h"
#include <stdint.h>

/*
* LM75 Temperature Sensor
*/

int16_t LM75::readRaw()
{
    uint8_t readBuf[2]; // 2 byte buffer to store the data read from the I2C device
    int16_t val; // Stores the 16 bit value of our ADC conversion

    startTimer();

    readBuf[0] = 0;
    readBuf[1] = 0;

    read(readBuf, 2); // Read the config register into readBuf

    val = ((int16_t)readBuf[0] << 8) | readBuf[1];
    fLastRawValue = val;

    stopTimer();

    return val;
}

bool LM75::devicePresent()
{
    uint8_t readBuf[2]; // 2 byte buffer to store the data read from the I2C device
    readBuf[0] = 0;
    readBuf[1] = 0;
    int n = read(readBuf, 2); // Read the data register into readBuf
    return (n == 2);
}

double LM75::getTemperature()
{
    fLastTemp = (double)readRaw() / 256.;
    return fLastTemp;
}
