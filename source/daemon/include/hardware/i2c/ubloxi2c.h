#ifndef _UBLOXI2C_H_
#define _UBLOXI2C_H_

#include "hardware/i2c/i2cdevice.h"

/* Ublox GPS receiver, I2C interface  */

class UbloxI2c : public i2cDevice {
public:
    UbloxI2c()
        : i2cDevice(0x42)
    {
        fTitle = "UBLOX I2C";
    }
    UbloxI2c(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "UBLOX I2C";
    }
    UbloxI2c(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        fTitle = "UBLOX I2C";
    }
    bool devicePresent();
    std::string getData();
    bool getTxBufCount(uint16_t& nrBytes);

private:
};

#endif // !_UBLOXI2C_H_
