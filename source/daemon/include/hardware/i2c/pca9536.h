#ifndef _PCA9536_H_
#define _PCA9536_H_
#include "hardware/i2c/i2cdevice.h"

/* PCA9536  */

class PCA9536 : public i2cDevice {
    // the device supports reading the incoming logic levels of the pins if set to input in the configuration register (will probably not use this feature)
    // the device supports polarity inversion (by configuring the polarity inversino register) (will probably not use this feature)
public:
    enum CFG_REG { INPUT_REG = 0x00,
        OUTPUT_REG = 0x01,
        POLARITY_INVERSION = 0x02,
        CONFIG_REG = 0x03 };
    enum CFG_PORT { C0 = 0,
        C1 = 2,
        C3 = 4,
        C4 = 8 };
    PCA9536()
        : i2cDevice(0x41)
    {
        fTitle = "PCA9536";
    }
    PCA9536(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "PCA9536";
    }
    PCA9536(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        fTitle = "PCA9536";
    }
    bool setOutputPorts(uint8_t portMask);
    bool setOutputState(uint8_t portMask);
    uint8_t getInputState();
    bool devicePresent();
};

#endif // !_PCA9536_H_
