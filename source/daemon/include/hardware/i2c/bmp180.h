#ifndef _BMP180_H_
#define _BMP180_H_

#include "hardware/i2c/i2cdevice.h"

/* BMP180  */

class BMP180 : public i2cDevice {
public:
    BMP180()
        : i2cDevice(0x77)
    {
        fTitle = "BMP180";
    }
    BMP180(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "BMP180";
    }
    BMP180(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        fTitle = "BMP180";
    }

    bool init();
    void readCalibParameters();
    inline bool isCalibValid() const { return fCalibrationValid; }
    signed int getCalibParameter(unsigned int param) const;
    unsigned int readUT();
    unsigned int readUP(uint8_t oss);
    double getTemperature();
    double getPressure(uint8_t oss);

private:
    unsigned int fLastConvTime;
    bool fCalibrationValid;
    signed int fCalibParameters[11];
};

#endif // _I2CDEVICES_H_
