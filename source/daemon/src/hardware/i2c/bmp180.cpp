#include "hardware/i2c/bmp180.h"
#include <stdint.h>
#include <stdio.h>

/*
* BMP180 Pressure Sensor
*/

bool BMP180::init()
{
    uint8_t val = 0;

    fCalibrationValid = false;

    // chip id reg
    int n = readReg(0xd0, &val, 1); // Read the id register into readBuf

    if (fDebugLevel > 1) {
        printf("%d bytes read\n", n);
        printf("chip id: 0x%x \n", val);
    }
    if (val == 0x55)
        readCalibParameters();
    return (val == 0x55);
}

unsigned int BMP180::readUT()
{
    uint8_t readBuf[2]; // 2 byte buffer to store the data read from the I2C device
    unsigned int val; // Stores the 16 bit value of our ADC conversion

    // start temp measurement: CR -> 0x2e
    uint8_t cr_val = 0x2e;
    // register address control reg
    int n = writeReg(0xf4, &cr_val, 1);

    // wait at least 4.5 ms
    usleep(4500);

    readBuf[0] = 0;
    readBuf[1] = 0;

    // adc reg
    n = readReg(0xf6, readBuf, 2); // Read the config register into readBuf
    if (fDebugLevel > 1)
        printf("%d bytes read\n", n);

    val = (readBuf[0]) << 8 | readBuf[1];

    return val;
}

unsigned int BMP180::readUP(uint8_t oss)
{
    uint8_t readBuf[3]; // 2 byte buffer to store the data read from the I2C device
    unsigned int val; // Stores the 16 bit value of our ADC conversion
    static const int delay[4] = { 4500, 7500, 13500, 25500 };

    // start pressure measurement: CR -> 0x34 | oss<<6
    uint8_t cr_val = 0x34 | (oss & 0x03) << 6;
    // register address control reg
    int n = writeReg(0xf4, &cr_val, 1);
    usleep(delay[oss & 0x03]);

    readBuf[0] = 0;
    readBuf[1] = 0;
    readBuf[2] = 0;

    // adc reg
    n = readReg(0xf6, readBuf, 3); // Read the conversion result into readBuf
    if (fDebugLevel > 1)
        printf("%d bytes read\n", n);

    val = (readBuf[0] << 16 | readBuf[1] << 8 | readBuf[2]) >> (8 - (oss & 0x03));

    return val;
}

signed int BMP180::getCalibParameter(unsigned int param) const
{
    if (param < 11)
        return fCalibParameters[param];
    return 0xffff;
}

void BMP180::readCalibParameters()
{
    uint8_t readBuf[22];
    // register address first byte eeprom
    int n = readReg(0xaa, readBuf, 22); // Read the 11 eeprom word values into readBuf
    if (fDebugLevel > 1)
        printf("%d bytes read\n", n);

    if (fDebugLevel > 1)
        printf("BMP180 eeprom calib data:\n");

    bool ok = true;
    for (int i = 0; i < 11; i++) {
        if (i > 2 && i < 6)
            fCalibParameters[i] = (uint16_t)(readBuf[2 * i] << 8 | readBuf[2 * i + 1]);
        else
            fCalibParameters[i] = (int16_t)(readBuf[2 * i] << 8 | readBuf[2 * i + 1]);
        if (fCalibParameters[i] == 0 || fCalibParameters[i] == 0xffff)
            ok = false;
        if (fDebugLevel > 1)
            printf("calib%d: %d \n", i, fCalibParameters[i]);
    }
    if (fDebugLevel > 1) {
        if (ok)
            printf("calib data is valid.\n");
        else
            printf("calib data NOT valid!\n");
    }

    fCalibrationValid = ok;
}

double BMP180::getTemperature()
{
    if (!fCalibrationValid)
        return -999.;
    signed int UT = readUT();
    signed int X1 = ((UT - fCalibParameters[5]) * fCalibParameters[4]) >> 15;
    signed int X2 = (fCalibParameters[9] << 11) / (X1 + fCalibParameters[10]);
    signed int B5 = X1 + X2;
    double T = (B5 + 8) / 160.;
    if (fDebugLevel > 1) {
        printf("UT=%d\n", UT);
        printf("X1=%d\n", X1);
        printf("X2=%d\n", X2);
        printf("B5=%d\n", B5);
        printf("Temp=%f\n", T);
    }
    return T;
}

double BMP180::getPressure(uint8_t oss)
{
    if (!fCalibrationValid)
        return 0.;
    signed int UT = readUT();
    if (fDebugLevel > 1)
        printf("UT=%d\n", UT);
    signed int UP = readUP(oss);
    if (fDebugLevel > 1)
        printf("UP=%d\n", UP);
    signed int X1 = ((UT - fCalibParameters[5]) * fCalibParameters[4]) >> 15;
    signed int X2 = (fCalibParameters[9] << 11) / (X1 + fCalibParameters[10]);
    signed int B5 = X1 + X2;
    signed int B6 = B5 - 4000;
    if (fDebugLevel > 1)
        printf("B6=%d\n", B6);
    X1 = (fCalibParameters[7] * ((B6 * B6) >> 12)) >> 11;
    if (fDebugLevel > 1)
        printf("X1=%d\n", X1);
    X2 = (fCalibParameters[1] * B6) >> 11;
    if (fDebugLevel > 1)
        printf("X2=%d\n", X2);
    signed int X3 = X1 + X2;
    signed int B3 = ((fCalibParameters[0] * 4 + X3) << (oss & 0x03)) + 2;
    B3 = B3 / 4;
    if (fDebugLevel > 1)
        printf("B3=%d\n", B3);
    X1 = (fCalibParameters[2] * B6) >> 13;
    if (fDebugLevel > 1)
        printf("X1=%d\n", X1);
    X2 = (fCalibParameters[6] * (B6 * B6) / 4096);
    X2 = X2 >> 16;
    if (fDebugLevel > 1)
        printf("X2=%d\n", X2);
    X3 = (X1 + X2 + 2) / 4;
    if (fDebugLevel > 1)
        printf("X3=%d\n", X3);
    unsigned long B4 = (fCalibParameters[3] * (unsigned long)(X3 + 32768)) >> 15;
    if (fDebugLevel > 1)
        printf("B4=%ld\n", B4);
    unsigned long B7 = ((unsigned long)UP - B3) * (50000 >> (oss & 0x03));
    if (fDebugLevel > 1)
        printf("B7=%ld\n", B7);
    int p = 0;
    if (B7 < 0x80000000)
        p = (B7 * 2) / B4;
    else
        p = (B7 / B4) * 2;
    if (fDebugLevel > 1)
        printf("p=%d\n", p);

    X1 = p >> 8;
    if (fDebugLevel > 1)
        printf("X1=%d\n", X1);
    X1 = X1 * X1;
    if (fDebugLevel > 1)
        printf("X1=%d\n", X1);
    X1 = (X1 * 3038) >> 16;
    if (fDebugLevel > 1)
        printf("X1=%d\n", X1);
    X2 = (-7357 * p) >> 16;
    if (fDebugLevel > 1)
        printf("X2=%d\n", X2);
    p = 16 * p + (X1 + X2 + 3791);
    double press = p / 16.;

    if (fDebugLevel > 1) {
        printf("pressure=%f\n", press);
    }
    return press;
}
