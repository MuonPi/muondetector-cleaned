#ifndef _SHT31_H_
#define _SHT31_H_

#include "hardware/i2c/i2cdevice.h"

/* SHT21  */

class SHT31 : public i2cDevice {
public:
	SHT31() : i2cDevice(0x44) { fTitle = "SHT31"; }
	SHT31(const char* busAddress, uint8_t slaveAddress) : i2cDevice(busAddress, slaveAddress) { fTitle = "SHT31"; }
	SHT31(uint8_t slaveAddress) : i2cDevice(slaveAddress) { fTitle = "SHT31"; }

	bool readRaw(uint16_t &UT, uint16_t &UH);  //read temperature; nothing gets passed
	bool breakCommand();
	bool softReset();                 //reset, datasheet, page 9, Rückgabetyp in void geändert
	bool heater(bool on);
	bool getValues(float &ftemp, float &fhum);
	//float getTemperature();   // calculates the temperature with the formula in the datasheet. Gets the solution of read_temp()
	//float getHumidity();    // calculates the temperature with the formula in the datasheet. Gets the solution of read_hum();

private:
	bool checksumCorrect(uint8_t data[]); // expects 3 byte long data array; Source: https://www2.htw-dresden.de/~wiki_sn/index.php/SHT21
};

#endif //!_SHT31_H_
