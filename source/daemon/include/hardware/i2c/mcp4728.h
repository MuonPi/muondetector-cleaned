#ifndef _MCP4728_H_
#define _MCP4728_H_
#include "hardware/i2c/i2cdevice.h"

/* MCP4728  */

class MCP4728 : public i2cDevice {
    // the DAC supports writing to input register but not sending latch bit to update the output register
    // here we will always send the "UDAC" (latch) bit because we don't need this functionality
    // MCP4728 listens to I2C Generall Call Commands
    // reset, wake-up, software update, read address bits
    // reset is "0x00 0x06"
    // wake-up is "0x00 0x09"
public:
    // to ground to power down the circuit
    const static float VDD; // change, if device powered with different voltage
    enum CFG_GAIN { GAIN1 = 0,
        GAIN2 = 1 };
    enum CFG_VREF { VREF_VDD = 0,
        VREF_2V = 1 };

    // struct that characterizes one dac output channel
    // setting the eeprom flag enables access to the eeprom registers instead of the dac output registers
    struct DacChannel {
        uint8_t pd = 0x00;
        CFG_GAIN gain = GAIN1;
        CFG_VREF vref = VREF_VDD;
        bool eeprom = false;
        uint16_t value = 0;
    };

    MCP4728()
        : i2cDevice(0x60)
    {
        fTitle = "MCP4728";
    }
    MCP4728(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "MCP4728";
    }
    MCP4728(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        fTitle = "MCP4728";
    }
    bool devicePresent();
    bool setVoltage(uint8_t channel, float voltage, bool toEEPROM = false);
    bool setValue(uint8_t channel, uint16_t value, uint8_t gain = GAIN1, bool toEEPROM = false);
    bool writeChannel(uint8_t channel, const DacChannel& channelData);
    bool readChannel(uint8_t channel, DacChannel& channelData);

    static float code2voltage(const DacChannel& channelData);

private:
    unsigned int fLastConvTime;
};

#endif //!_MCP4728_H_
