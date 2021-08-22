#ifndef _SHT21_H_
#define _SHT21_H_

#include "hardware/i2c/i2cdevice.h"

/* SHT21  */

class SHT21 : public i2cDevice {
public:
    SHT21()
        : i2cDevice(0x40)
    {
        fTitle = "SHT21";
    }
    SHT21(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "SHT21";
    }
    SHT21(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        fTitle = "SHT21";
    }

    uint16_t readUT(); //read temperature; nothing gets passed
    uint16_t readUH(); //read humidity;  nothing gets passed

    bool softReset(); //reset, datasheet, page 9, Rückgabetyp in void geändert
    uint8_t readResolutionSettings();
    void setResolutionSettings(uint8_t settingsByte); //Sets the resolution Bits for humidity and temperature
    float getTemperature(); // calculates the temperature with the formula in the datasheet. Gets the solution of read_temp()
    float getHumidity(); // calculates the temperature with the formula in the datasheet. Gets the solution of read_hum();

private:
    bool checksumCorrect(uint8_t data[]); // expects 3 byte long data array; Source: https://www2.htw-dresden.de/~wiki_sn/index.php/SHT21
};

#endif //!_SHT21_H_
