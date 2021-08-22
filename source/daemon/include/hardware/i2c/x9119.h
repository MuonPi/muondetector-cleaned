#ifndef _X9119_H_
#define _X9119_H_

#include "hardware/i2c/i2cdevice.h"

/* X9119  */

class X9119 : public i2cDevice {
public:
    X9119()
        : i2cDevice(0x28)
    {
        fTitle = "X9119";
    }
    X9119(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "X9119";
    }
    X9119(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        fTitle = "X9119";
    }

    unsigned int readWiperReg();
    unsigned int readWiperReg2();
    unsigned int readWiperReg3();
    void writeWiperReg(unsigned int value);
    unsigned int readDataReg(uint8_t reg);
    void writeDataReg(uint8_t reg, unsigned int value);

private:
    unsigned int fWiperReg;
};

#endif // !_X9119_H_
