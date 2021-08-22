#ifndef _HMC5883_H_
#define _HMC5883_H_

#include "hardware/i2c/i2cdevice.h"

/* HMC5883  */

class HMC5883 : public i2cDevice {
public:
    // Resolution for the 8 gain settings in mG/LSB
    static const double GAIN[8];
    HMC5883()
        : i2cDevice(0x1e)
    {
        fTitle = "HMC5883";
    }
    HMC5883(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "HMC5883";
    }
    HMC5883(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        fTitle = "HMC5883";
    }

    bool init();
    // gain range 0..7
    void setGain(uint8_t gain);
    uint8_t readGain();
    bool getXYZRawValues(int& x, int& y, int& z);
    bool getXYZMagneticFields(double& x, double& y, double& z);
    bool readRDYBit();
    bool readLockBit();
    bool calibrate(int& x, int& y, int& z);

private:
    unsigned int fLastConvTime;
    bool fCalibrationValid;
    unsigned int fGain;
    signed int fCalibParameters[11];
};

#endif // !_HMC5883_H_
