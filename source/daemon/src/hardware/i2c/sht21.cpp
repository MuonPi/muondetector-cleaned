#include "hardware/i2c/sht21.h"
#include <stdint.h>
#include <stdio.h>

/*
* SHT21 Temperature&Humidity Sensor
* Prefered option is the no hold mastermode
*/

bool SHT21::checksumCorrect(uint8_t data[]) // expects data to be greater or equal 3 (expecting 3)
{
    const uint16_t Polynom = 0x131;
    uint8_t crc = 0;
    uint8_t byteCtr;
    uint8_t bit;
    try {
        for (byteCtr = 0; byteCtr < 2; ++byteCtr) {
            crc ^= (data[byteCtr]);
            for (bit = 8; bit > 0; --bit) {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ Polynom;
                } else {
                    crc = (crc << 1);
                }
            }
        }
        if (crc != data[2]) {
            return false;
        } else {
            return true;
        }
    } catch (int i) {
        printf("Error, Array too small in function: checksumCorrect\n");
        return false;
    }
}

float SHT21::getHumidity()
{
    uint16_t data_hum = readUH();
    float fhum; //end calculation -> Humidity
    fhum = -6.0 + 125.0 * (((float)(data_hum)) / 65536.0);
    return (fhum);
}

float SHT21::getTemperature()
{
    uint16_t data_temp = readUT();
    float ftemp; //endl calculation -> Temperature
    ftemp = -46.85 + 175.72 * (((float)(data_temp)) / 65536.0);
    return (ftemp);
}

uint16_t SHT21::readUT() // von unsigned int auf float geändert
{
    uint8_t writeBuf[1];
    uint8_t readBuf[3];
    uint16_t data_temp;

    writeBuf[0] = 0;
    readBuf[0] = 0;
    readBuf[1] = 0;
    readBuf[2] = 0;
    data_temp = 0;

    writeReg(0xF3, writeBuf, 0);
    usleep(85000);
    while (read(readBuf, 3) != 3)
        ;

    if (fDebugLevel > 1) {
        printf("Inhalt: (MSB Byte): %x\n", readBuf[0]);
        printf("Inhalt: (LSB Byte): %x\n", readBuf[1]);
        printf("Inhalt: (Checksum Byte): %x\n", readBuf[2]);
    }

    data_temp = ((uint16_t)readBuf[0]) << 8;
    data_temp |= readBuf[1];
    data_temp &= 0xFFFC; //Vergleich mit 0xFC um die letzten beiden Bits auf 00 zu setzen.

    if (!checksumCorrect(readBuf)) {
        printf("checksum error\n");
    }
    return data_temp;
}

uint16_t SHT21::readUH() //Hold mode
{
    uint8_t writeBuf[1];
    uint8_t readBuf[3]; //what should be read later
    uint16_t data_hum;

    writeBuf[0] = 0;
    readBuf[0] = 0;
    readBuf[1] = 0;
    readBuf[2] = 0;
    data_hum = 0;

    writeReg(0xF5, writeBuf, 0);
    usleep(30000);
    while (read(readBuf, 3) != 3)
        ;

    if (fDebugLevel > 1) {
        printf("Es wurden gelesen (MSB Bytes): %x\n", readBuf[0]);
        printf("Es wurden gelesen (LSB Bytes): %x\n", readBuf[1]);
        printf("Es wurden gelesen (Checksum Bytes): %x\n", readBuf[2]);
    }

    data_hum = ((uint16_t)readBuf[0]) << 8;
    data_hum |= readBuf[1];
    data_hum &= 0xFFFC; //Vergleich mit 0xFC um die letzten beiden Bits auf 00 zu setzen.

    if (!checksumCorrect(readBuf)) {
        printf("checksum error\n");
    }
    return data_hum;
}

uint8_t SHT21::readResolutionSettings()
{ //reads the temperature and humidity resolution settings byte
    uint8_t readBuf[1];

    readBuf[0] = 0; //Initialization

    readReg(0xE7, readBuf, 1);
    return readBuf[0];
}

void SHT21::setResolutionSettings(uint8_t settingsByte)
{ //sets the temperature and humidity resolution settings byte
    uint8_t readBuf[1];
    readBuf[0] = 0; //Initialization
    readReg(0xE7, readBuf, 1);
    readBuf[0] &= 0b00111000; // mask, to not change reserved bits (Bit 3, 4 and 5 are reserved)
    settingsByte &= 0b11000111;
    uint8_t writeBuf[1];
    writeBuf[0] = settingsByte | readBuf[0];
    writeReg(0xE6, writeBuf, 1);
}

bool SHT21::softReset()
{
    uint8_t writeBuf[1];
    writeBuf[0] = 0xFE;

    int n = 0;
    n = writeReg(0xFE, writeBuf, 0);
    usleep(15000); //wait for the SHT to reset; datasheet on page 9

    if (n == 0) {
        printf("soft_reset succesfull %i\n", n);
    }

    return (n == 0); //Wenn n == 0, gibt die Funktion True zurück. Wenn nicht gibt sie False zurück.
}
