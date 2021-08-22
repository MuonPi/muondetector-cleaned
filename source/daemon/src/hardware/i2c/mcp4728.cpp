#include "hardware/i2c/mcp4728.h"
#include <stdint.h>

/*
* MCP4728 4 ch 12 bit DAC
*/

const float MCP4728::VDD = 3.3; // change, if device powered with different voltage

bool MCP4728::setVoltage(uint8_t channel, float voltage, bool toEEPROM)
{
    // Vout = (2.048V * Dn) / 4096 * Gx <= VDD
    if (voltage < 0) {
        return false;
    }
    uint8_t gain = 0;
    unsigned int value = (int)(voltage * 2000 + 0.5);
    if (value > 0xfff) {
        value = value >> 1;
        gain = 1;
    }
    if (value > 0xfff) {
        // error message
        return false;
    }
    return setValue(channel, value, gain, toEEPROM);
}

bool MCP4728::setValue(uint8_t channel, uint16_t value, uint8_t gain, bool toEEPROM)
{
    if (value > 0xfff) {
        value = 0xfff;
        // error number of bits exceeding 12
        return false;
    }
    startTimer();
    channel = channel & 0x03;
    uint8_t buf[3];
    if (toEEPROM) {
        buf[0] = 0b01011001;
    } else {
        buf[0] = 0b01000001;
    }
    buf[0] = buf[0] | (channel << 1); // 01000/01011 (multiwrite/singlewrite command) DAC1 DAC0 (channel) UDAC bit =1
    buf[1] = 0b10000000 | (uint8_t)((value & 0xf00) >> 8); // Vref PD1 PD0 Gx (gain) D11 D10 D9 D8
    buf[1] = buf[1] | (gain << 4);
    buf[2] = (uint8_t)(value & 0xff); // D7 D6 D5 D4 D3 D2 D1 D0
    if (write(buf, 3) != 3) {
        // somehow did not write exact same amount of bytes as it should
        return false;
    }
    stopTimer();
    fLastConvTime = fLastTimeInterval;
    return true;
}

bool MCP4728::writeChannel(uint8_t channel, const DacChannel& channelData)
{
    if (channelData.value > 0xfff) {
        // error number of bits exceeding 12
        return false;
    }
    startTimer();
    channel = channel & 0x03;
    uint8_t buf[3];
    if (channelData.eeprom) {
        buf[0] = 0b01011001;
    } else {
        buf[0] = 0b01000001;
    }
    buf[0] |= (channel << 1); // 01000/01011 (multiwrite/singlewrite command) DAC1 DAC0 (channel) UDAC bit =1
    buf[1] = ((uint8_t)channelData.vref) << 7; // Vref PD1 PD0 Gx (gain) D11 D10 D9 D8
    buf[1] |= (channelData.pd & 0x03) << 5;
    buf[1] |= (uint8_t)((channelData.value & 0xf00) >> 8);
    buf[1] |= (uint8_t)(channelData.gain & 0x01) << 4;
    buf[2] = (uint8_t)(channelData.value & 0xff); // D7 D6 D5 D4 D3 D2 D1 D0
    if (write(buf, 3) != 3) {
        // somehow did not write exact same amount of bytes as it should
        return false;
    }
    stopTimer();
    return true;
}

bool MCP4728::devicePresent()
{
    uint8_t buf[24];
    startTimer();
    // perform a read sequence of all registers as described in datasheet
    int n = read(buf, 24);
    stopTimer();
    return (n == 24);
}

bool MCP4728::readChannel(uint8_t channel, DacChannel& channelData)
{
    if (channel > 3) {
        // error: channel index exceeding 3
        return false;
    }

    startTimer();

    uint8_t buf[24];
    if (read(buf, 24) != 24) {
        // somehow did not read exact same amount of bytes as it should
        stopTimer();
        return false;
    }
    uint8_t offs = (channelData.eeprom == false) ? 1 : 4;
    channelData.vref = (buf[channel * 6 + offs] & 0x80) ? VREF_2V : VREF_VDD;
    channelData.pd = (buf[channel * 6 + offs] & 0x60) >> 5;
    channelData.gain = (buf[channel * 6 + offs] & 0x10) ? GAIN2 : GAIN1;
    channelData.value = (uint16_t)(buf[channel * 6 + offs] & 0x0f) << 8;
    channelData.value |= (uint16_t)(buf[channel * 6 + offs + 1] & 0xff);

    stopTimer();

    return true;
}

float MCP4728::code2voltage(const DacChannel& channelData)
{
    float vref = (channelData.vref == VREF_2V) ? 2.048 : VDD;
    float voltage = vref * channelData.value / 4096;
    if (channelData.gain == GAIN2 && channelData.vref != VREF_VDD)
        voltage *= 2.;
    return voltage;
}
