#include "hardware/i2c/sht31.h"
#include <stdint.h>
#include <stdio.h>

/*
* SHT31 Temperature&Humidity Sensor
* Prefered option is the no hold mastermode
*/

bool SHT31::checksumCorrect(uint8_t data[]) // expects data to be greater or equal 3 (expecting 3)
{
	const uint16_t Polynom = 0x131;
	uint8_t crc = 0xff;
	uint8_t byteCtr;
	uint8_t bit;
	try {
		for (byteCtr = 0; byteCtr < 2; ++byteCtr)
		{
			crc ^= (data[byteCtr]);
			for (bit = 8; bit > 0; --bit)
			{
				if (crc & 0x80)
				{
					crc = (crc << 1) ^ Polynom;
				}
				else
				{
					crc = (crc << 1);
				}
			}
		}
		if (crc != data[2])
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	catch (int i) {
		printf("Error, Array too small in function: checksumCorrect\n");
		return false;
	}
}

bool SHT31::getValues(float &ftemp, float &fhum) {
	uint16_t data_hum = 0;
	uint16_t data_temp = 0;
	if (!readRaw(data_temp, data_hum)) {
		return false;
	}
	ftemp = -45. + 175. * (((float)((data_temp))) / 65535.0);
	fhum = 100.0 * (((float)(data_hum)) / 65535.0);
	return true;
}

bool SHT31::readRaw(uint16_t &UT, uint16_t &UH)  // von unsigned int auf float geändert
{
	uint8_t writeBuf[2];
	uint8_t readBuf[6];

	writeBuf[0] = 0x24;
	writeBuf[1] = 0x00;
	for (int i = 0; i < 6; i++) {
		readBuf[i] = 0;
	}

	write(writeBuf, 2);
	usleep(20000);
	if (read(readBuf, 6) != 6) {
		printf("error, not 6 bytes read");
		return false;
	}

	if (fDebugLevel > 1)
	{
		printf("Inhalt: (MSB Byte T): %x\n", readBuf[0]);
		printf("Inhalt: (LSB Byte T): %x\n", readBuf[1]);
		printf("Inhalt: (Checksum Byte T): %x\n", readBuf[2]);
		printf("Inhalt: (MSB Byte H): %x\n", readBuf[3]);
		printf("Inhalt: (LSB Byte H): %x\n", readBuf[4]);
		printf("Inhalt: (Checksum Byte H): %x\n", readBuf[5]);
	}

	UT = ((uint16_t)readBuf[0]) << 8;
	UT |= readBuf[1];
	UH = ((uint16_t)readBuf[3]) << 8;
	UH |= readBuf[4];
	


	if (!checksumCorrect(readBuf))
	{
		printf("temperature checksum error\n");
	}
	if (!checksumCorrect((uint8_t*)(readBuf + 3)))
	{
		printf("humidity checksum error\n");
	}
	return true;
}

bool SHT31::heater(bool on) {
	uint8_t writeBuf[2];
	writeBuf[0] = 0x30;
	writeBuf[1] = 0x66;
	if (on) {
		writeBuf[1] = 0x6D;
	}
	int n = write(writeBuf, 2);
	if (n != 2) {
		return false;
	}
	return true;
}

bool SHT31::breakCommand() {
	uint8_t writeBuf[2];
	writeBuf[0] = 0x30;
	writeBuf[1] = 0x93;

	int n = 0;
	n = write(writeBuf, 2);
	usleep(15000);
}

bool SHT31::softReset()
{
	uint8_t writeBuf[2];
	writeBuf[0] = 0x30;
	writeBuf[1] = 0xA2;

	int n = 0;
	n = write(writeBuf, 2);
	usleep(15000);  //wait for the SHT to reset; datasheet on page 9

	if (n == 2)
	{
		printf("soft_reset succesfull %i\n", n);
	}

	return(n == 0);  //Wenn n == 0, gibt die Funktion True zurück. Wenn nicht gibt sie False zurück.
}
