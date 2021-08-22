#include "hardware/i2c/hmc5883.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/*
* HMC5883 3 axis magnetic field sensor (Honeywell)
*/

const double HMC5883::GAIN[8] = { 0.73, 0.92, 1.22, 1.52, 2.27, 2.56, 3.03, 4.35 };

bool HMC5883::init()
{
    uint8_t readBuf[3]; // 2 byte buffer to store the data read from the I2C device

    // init value 0 for gain
    fGain = 0;

    readBuf[0] = 0;
    readBuf[1] = 0;
    readBuf[2] = 0;

    int n = readReg(0x0a, readBuf, 3); // Read the id registers into readBuf

    if (fDebugLevel > 1) {
        printf("%d bytes read\n", n);
        printf("id reg A: 0x%x \n", readBuf[0]);
        printf("id reg B: 0x%x \n", readBuf[1]);
        printf("id reg C: 0x%x \n", readBuf[2]);
    }

    if (readBuf[0] != 0x48)
        return false;

    // addr config reg A (CRA)
    // 8 average, 15 Hz, single measurement: 0x70
    uint8_t cmd = 0x70;
    n = writeReg(0x00, &cmd, 1);

    setGain(fGain);
    return true;
}

void HMC5883::setGain(uint8_t gain)
{
    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device

    // set CRB
    writeBuf[0] = 0x01; // addr config reg B (CRB)
    writeBuf[1] = (gain & 0x07) << 5; // gain=xx
    write(writeBuf, 2);
    fGain = gain & 0x07;
}

uint8_t HMC5883::readGain()
{
    uint8_t readBuf[3]; // 2 byte buffer to store the data read from the I2C device

    int n = readReg(0x01, readBuf, 1); // Read the config register into readBuf

    if (n != 1)
        return 0;
    uint8_t gain = readBuf[0] >> 5;
    if (fDebugLevel > 1) {
        printf("%d bytes read\n", n);
        printf("gain (read from device): 0x%x\n", gain);
    }
    return gain;
}

bool HMC5883::readRDYBit()
{
    uint8_t readBuf[3]; // 2 byte buffer to store the data read from the I2C device

    // addr status reg (SR)
    int n = readReg(0x09, readBuf, 1); // Read the status register into readBuf

    if (n != 1)
        return 0;
    uint8_t sr = readBuf[0];
    if (fDebugLevel > 1) {
        printf("%d bytes read\n", n);
        printf("status (read from device): 0x%x\n", sr);
    }
    if ((sr & 0x01) == 0x01)
        return true;
    return false;
}

bool HMC5883::readLockBit()
{
    uint8_t readBuf[3]; // 2 byte buffer to store the data read from the I2C device

    // addr status reg (SR)
    int n = readReg(0x09, readBuf, 1); // Read the status register into readBuf

    if (n != 1)
        return 0;
    uint8_t sr = readBuf[0];
    if (fDebugLevel > 1) {
        printf("%d bytes read\n", n);
        printf("status (read from device): 0x%x\n", sr);
    }
    if ((sr & 0x02) == 0x02)
        return true;
    return false;
}

bool HMC5883::getXYZRawValues(int& x, int& y, int& z)
{
    uint8_t readBuf[6];

    uint8_t cmd = 0x01; // start single measurement
    int n = writeReg(0x02, &cmd, 1); // addr mode reg (MR)
    usleep(6000);

    // Read the 3 data registers into readBuf starting from addr 0x03
    n = readReg(0x03, readBuf, 6);
    int16_t xreg = (int16_t)(readBuf[0] << 8 | readBuf[1]);
    int16_t yreg = (int16_t)(readBuf[2] << 8 | readBuf[3]);
    int16_t zreg = (int16_t)(readBuf[4] << 8 | readBuf[5]);

    if (fDebugLevel > 1) {
        printf("%d bytes read\n", n);
        printf("xreg: %d\n", xreg);
        printf("yreg: %d\n", yreg);
        printf("zreg: %d\n", zreg);
    }

    x = xreg;
    y = yreg;
    z = zreg;

    if (xreg >= -2048 && xreg < 2048 && yreg >= -2048 && yreg < 2048 && zreg >= -2048 && zreg < 2048)
        return true;

    return false;
}

bool HMC5883::getXYZMagneticFields(double& x, double& y, double& z)
{
    int xreg, yreg, zreg;
    bool ok = getXYZRawValues(xreg, yreg, zreg);
    double lsbgain = GAIN[fGain];
    x = lsbgain * xreg / 1000.;
    y = lsbgain * yreg / 1000.;
    z = lsbgain * zreg / 1000.;

    if (fDebugLevel > 1) {
        printf("x field: %f G\n", x);
        printf("y field: %f G\n", y);
        printf("z field: %f G\n", z);
    }

    return ok;
}

bool HMC5883::calibrate(int& x, int& y, int& z)
{

    // addr config reg A (CRA)
    // 8 average, 15 Hz, positive self test measurement: 0x71
    uint8_t cmd = 0x71;
    /*int n=*/writeReg(0x00, &cmd, 1);

    uint8_t oldGain = fGain;
    setGain(5);

    int xr, yr, zr;
    // one dummy measurement
    getXYZRawValues(xr, yr, zr);
    // measurement
    getXYZRawValues(xr, yr, zr);

    x = xr;
    y = yr;
    z = zr;

    setGain(oldGain);
    // one dummy measurement
    getXYZRawValues(xr, yr, zr);

    // set normal measurement mode in CRA again
    cmd = 0x70;
    /*n=*/writeReg(0x00, &cmd, 1);
    return true;
}
