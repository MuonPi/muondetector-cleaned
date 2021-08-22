#include "hardware/i2c/ubloxi2c.h"
#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <string>

/* Ublox GPS receiver, I2C interface */

bool UbloxI2c::devicePresent()
{
    uint8_t dummy;
    return (1 == readReg(0xff, &dummy, 1));
}

std::string UbloxI2c::getData()
{
    uint16_t nrBytes = 0;
    if (!getTxBufCount(nrBytes))
        return "";
    if (nrBytes == 0)
        return "";
    uint8_t reg = 0xff;
    int n = write(&reg, 1);
    if (n != 1)
        return "";
    uint8_t buf[128];
    if (nrBytes > 128)
        nrBytes = 128;
    n = read(buf, nrBytes);
    if (n != nrBytes)
        return "";
    std::string str(nrBytes, ' ');
    std::transform(&buf[0], &buf[nrBytes], str.begin(), [](uint8_t c) { return (unsigned char)c; });
    return str;
}

bool UbloxI2c::getTxBufCount(uint16_t& nrBytes)
{
    startTimer();
    uint8_t reg = 0xfd;
    uint8_t buf[2];
    int n = write(&reg, 1);
    if (n != 1)
        return false;
    n = read(buf, 2); // Read the data at TX buf counter location
    stopTimer();
    if (n != 2)
        return false;
    uint16_t temp = buf[1];
    temp |= (uint16_t)buf[0] << 8;
    nrBytes = temp;
    return true;
}
