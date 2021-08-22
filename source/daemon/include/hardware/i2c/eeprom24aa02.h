#ifndef _EEPROM24AA02_H_
#define _EEPROM24AA02_H_

#include "hardware/i2c/i2cdevice.h"

/* EEPROM24AA02  */

class EEPROM24AA02 : public i2cDevice {
public:
    EEPROM24AA02()
        : i2cDevice(0x50)
    {
        fTitle = "24AA02";
    }
    EEPROM24AA02(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "24AA02";
    }
    EEPROM24AA02(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        fTitle = "24AA02";
    }

    uint8_t readByte(uint8_t addr);
    bool readByte(uint8_t addr, uint8_t* value);
    void writeByte(uint8_t addr, uint8_t data);
    bool writeBytes(uint8_t addr, uint16_t length, uint8_t* data);

private:
};
#endif //!_EEPROM24AA02_H
