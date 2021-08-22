#include "hardware/i2c/eeprom24aa02.h"
#include <stdint.h>
#include <unistd.h>

/*
* 24AA02 EEPROM
*/

uint8_t EEPROM24AA02::readByte(uint8_t addr)
{
    uint8_t val = 0;
    startTimer();
    readReg(addr, &val, 1); // Read the data at address location
    stopTimer();
    return val;
}

bool EEPROM24AA02::readByte(uint8_t addr, uint8_t* value)
{
    startTimer();
    int n = readReg(addr, value, 1); // Read the data at address location
    stopTimer();
    return (n == 1);
}

void EEPROM24AA02::writeByte(uint8_t addr, uint8_t data)
{
    uint8_t writeBuf[2]; // Buffer to store the 2 bytes that we write to the I2C device

    writeBuf[0] = addr; // address of data byte
    writeBuf[1] = data; // data byte

    startTimer();

    // Write address first
    write(writeBuf, 2);

    usleep(5000);
    stopTimer();
}

/** Write multiple bytes to starting from given address into EEPROM memory.
* @param addr First register address to write to
* @param length Number of bytes to write
* @param data Buffer to copy new data from
* @return Status of operation (true = success)
* @note this is an overloaded function to the one from the i2cdevice base class in order to
* prevent sequential write operations crossing page boundaries of the EEPROM. This function conforms to
* the page-wise sequential write (c.f. http://ww1.microchip.com/downloads/en/devicedoc/21709c.pdf  p.7).
*/
bool EEPROM24AA02::writeBytes(uint8_t addr, uint16_t length, uint8_t* data)
{

    static const uint8_t PAGESIZE = 8;
    bool success = true;
    startTimer();
    for (uint16_t i = 0; i < length;) {
        uint8_t currAddr = addr + i;
        // determine, how many bytes left on current page
        uint8_t pageRemainder = PAGESIZE - currAddr % PAGESIZE;
        if (currAddr + pageRemainder >= length)
            pageRemainder = length - currAddr;
        int n = writeReg(currAddr, &data[i], pageRemainder);
        usleep(5000);
        i += pageRemainder;
        success = success && (n == pageRemainder);
    }
    stopTimer();
    return success;
}
