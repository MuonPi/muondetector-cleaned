#ifndef _LM75_H_
#define _LM75_H_
#include "hardware/i2c/i2cdevice.h"
class LM75 : public i2cDevice {
public:
    LM75()
        : i2cDevice(0x4f)
    {
        fTitle = "LM75A";
    }
    LM75(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "LM75A";
    }
    LM75(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        fTitle = "LM75A";
    }
    bool devicePresent();
    int16_t readRaw();
    double getTemperature();
    double lastTemperatureValue() const { return fLastTemp; }

private:
    unsigned int fLastConvTime;
    signed int fLastRawValue;
    double fLastTemp;
};
#endif // !_LM75_H_
