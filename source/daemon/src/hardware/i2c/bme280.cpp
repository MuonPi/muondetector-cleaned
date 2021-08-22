#include "hardware/i2c/bme280.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
/*
*BME280 HumidityTemperaturePressuresensor
*
*/
bool BME280::init()
{
    fTitle = "BME280";

    uint8_t val = 0;
    fCalibrationValid = false;

    // chip id reg
    int n = readReg(0xd0, &val, 1); // Read the id register into readBuf
        //  printf( "%d bytes read\n",n);

    if (fDebugLevel > 1) {
        printf("%d bytes read\n", n);
        printf("chip id: 0x%x \n", val);
    }

    if (val == 0x60)
        readCalibParameters();
    return (val == 0x60);
}

bool BME280::status()
{
    uint8_t status = 0;
    int n = readReg(0xf3, &status, 1);
    if (fDebugLevel > 1) {
        printf("%d bytes read\n", n);
    }
    status &= 0b1001;
    return (status == 0 && n == 1);
}

uint8_t BME280::readConfig()
{
    uint8_t config[1];
    config[0] = 0;
    int n = readReg(0xf5, config, 1);
    if (fDebugLevel > 1)
        printf("%d bytes read\n", n);
    return (unsigned int)config[0];
}

uint8_t BME280::read_CtrlMeasReg()
{
    uint8_t ctrl_meas[1];
    ctrl_meas[0] = 0;
    int n = readReg(0xf4, ctrl_meas, 1);
    if (fDebugLevel > 1)
        printf("%d bytes read\n", n);
    return (uint8_t)ctrl_meas[0];
}

bool BME280::writeConfig(uint8_t config)
{
    uint8_t buf[1];
    // check for bit 1 because datasheet says: "do not change"
    int n = readReg(0xf5, buf, 1);
    buf[0] = buf[0] & 0b10;
    config = config & 0b11111101;
    buf[0] = buf[0] | config;
    n += writeReg(0xf5, buf, 1);
    if (fDebugLevel > 1)
        printf("%d bytes written\n", n);
    return (n == 2);
}

bool BME280::write_CtrlMeasReg(uint8_t config)
{
    uint8_t buf[1];
    buf[0] = config;
    int n = writeReg(0xf4, buf, 1);
    if (fDebugLevel > 1)
        printf("%d bytes written\n", n);
    return (n == 1);
}

bool BME280::setMode(uint8_t mode)
{ // 0= skipped; oversampling: 1: x1; 2:x2; 3: x4; 4:x8; 5:x16
    if (mode > 0b11) {
        if (fDebugLevel > 1)
            printf("mode > 3 error\n");
        return false;
    }
    uint8_t buf[1];
    int n = readReg(0xf4, buf, 1);
    uint8_t ctrl_meas = buf[0];
    ctrl_meas = ctrl_meas & 0xfc;
    ctrl_meas = ctrl_meas | mode;
    buf[0] = ctrl_meas;
    n += writeReg(0xf4, buf, 1);
    return (n == 2);
}

bool BME280::setTSamplingMode(uint8_t mode)
{ // 0= skipped; oversampling: 1: x1; 2:x2; 3: x4; 4:x8; 5:x16
    if (mode > 0b111) {
        if (fDebugLevel > 1)
            printf("mode > 3 error\n");
        return false;
    }
    uint8_t buf[1];
    int n = readReg(0xf4, buf, 1);
    uint8_t ctrl_meas = buf[0];
    ctrl_meas = ctrl_meas & 0b00011111;
    mode = mode << 5;
    ctrl_meas = ctrl_meas | mode;
    buf[0] = ctrl_meas;
    n += writeReg(0xf4, buf, 1);
    return (n == 2);
}

bool BME280::setPSamplingMode(uint8_t mode)
{ // 0= skipped; oversampling: 1: x1; 2:x2; 3: x4; 4:x8; 5:x16
    if (mode > 0b111) {
        if (fDebugLevel > 1)
            printf("mode > 3 error\n");
        return false;
    }
    uint8_t buf[1];
    int n = readReg(0xf4, buf, 1);
    uint8_t ctrl_meas = buf[0];
    ctrl_meas = ctrl_meas & 0b11100011;
    mode = mode << 2;
    ctrl_meas = ctrl_meas | mode;
    buf[0] = ctrl_meas;
    n += writeReg(0xf4, buf, 1);
    return (n == 2);
}

bool BME280::setHSamplingMode(uint8_t mode)
{ // 0= skipped; oversampling: 1: x1; 2:x2; 3: x4; 4:x8; 5:x16
    if (mode > 0b111) {
        if (fDebugLevel > 1)
            printf("mode > 3 error\n");
        return false;
    }
    uint8_t buf[1];
    int n = readReg(0xf2, buf, 1);
    uint8_t ctrl_meas = buf[0];
    ctrl_meas = ctrl_meas & 0b11111000;
    ctrl_meas = ctrl_meas | mode;
    buf[0] = ctrl_meas;
    n += writeReg(0xf2, buf, 1);
    n += readReg(0xf4, buf, 1);
    writeReg(0xf4, buf, 1);
    return (n == 2);
}

bool BME280::setDefaultSettings()
{
    uint8_t buf[1];
    readReg(0xf2, buf, 1);
    buf[0] &= 0b11111000;
    buf[0] |= 0b010;
    writeReg(0xf2, buf, 1); // enabling humidity measurement (oversampling x2)
    write_CtrlMeasReg(0b01001000); // enabling temperature and pressure measurement (oversampling x2), set sleep mode
    return true;
}

bool BME280::measure()
{
    // calculate t_max [ms] from settings:
    uint8_t readBuf[1];
    double t_max = 1.25;
    readReg(0xf2, readBuf, 1);
    unsigned int val = readBuf[0] & 0b111;
    if (fDebugLevel > 1)
        printf("osrs_h: %u\n", val);
    if (val > 5)
        val = 5;
    unsigned int add = 1;
    if (val != 0) {
        add = add << (val - 1);
        t_max += 2.3 * (double)add + 0.575;
    }

    readBuf[0] = 0;
    add = 1;
    readReg(0xf4, readBuf, 1);
    val = readBuf[0] & 0b00011100;
    val = val >> 2;
    if (fDebugLevel > 1)
        printf("osrs_p: %u\n", val);
    if (val > 5)
        val = 5;
    if (val != 0) {
        add = add << (val - 1);
        t_max += 2.3 * (double)add + 0.575;
    }

    add = 1;
    val = readBuf[0] & 0b11100000;
    val = val >> 5;
    if (fDebugLevel > 1)
        printf("osrs_t: %u\n", val);
    if (val > 5)
        val = 5;
    if (val != 0) {
        add = add << (val - 1);
        t_max += 2.3 * (double)add;
    }
    // t_max corresponds to the maximum time that a measurement can take with given
    // settings read out from registers f2 and f4

    // wait while status not ready:
    for (int i = 0; i < 10; i++) {
        usleep(5000);
    }
    setMode(0x2); // set mode to "forced measurement" (single-shot)
        // it will now perform a measurement as configured in 0xf4, 0xf2 and 0xf5 registers

    // wait at least 112.8 ms for a full accuracy measurement of all 3 values
    // or ask for status to be 0
    usleep((int)(t_max * 1000 + 0.5) + 200);
    if (fDebugLevel > 1)
        printf("measurement took about %.1f ms\n", t_max + 0.2);
    for (int i = 0; i < 10; i++) {
        if (status()) {
            break;
        }
        usleep(5000);
        if (i == 9) {
            return false;
        }
    }
    return true;
}

int32_t BME280::readUT()
{
    uint8_t readBuf[3]; // 2 byte buffer to store the data read from the I2C device
    uint32_t val; // Stores the 20 bit value of our ADC conversion

    if (!measure()) {
        std::cerr << "error: measurement invalid";
        return INT32_MIN;
    }

    readBuf[0] = 0;
    readBuf[1] = 0;
    readBuf[2] = 0;

    // adc reg
    int n = readReg(0xfa, readBuf, 3); // Read the config register into readBuf
    if (fDebugLevel > 1)
        printf("%d bytes read: 0x%02x 0x%02x 0x%02x\n", n, readBuf[0], readBuf[1], readBuf[2]);

    val = ((uint32_t)readBuf[0]) << 12; // <-------///// should the lower 4 bits or the higher 4 bits of the 24 bits of registers be left 0 ???
    val |= ((uint32_t)readBuf[1]) << 4; // (look datasheet page 25)
    val |= ((uint32_t)readBuf[2]) >> 4;

    return (int32_t)val;
}

int32_t BME280::readUP()
{
    uint8_t readBuf[3]; // 2 byte buffer to store the data read from the I2C device
    uint32_t val; // Stores the 20 bit value of our ADC conversion

    if (!measure()) {
        std::cerr << "error: measurement invalid";
        return INT32_MIN;
    }

    readBuf[0] = 0;
    readBuf[1] = 0;
    readBuf[2] = 0;

    // adc reg
    int n = readReg(0xf7, readBuf, 3); // Read the config register into readBuf
    if (fDebugLevel > 1)
        printf("%d bytes read\n", n);

    val = ((uint32_t)readBuf[0]) << 12;
    val |= ((uint32_t)readBuf[1]) << 4;
    val |= ((uint32_t)readBuf[2]) >> 4;

    return (int32_t)val;
}

int32_t BME280::readUH()
{
    uint8_t readBuf[2];
    uint16_t val;

    if (!measure()) {
        std::cerr << "error: measurement invalid";
        return INT32_MIN;
    }

    readBuf[0] = 0;
    readBuf[1] = 0;

    // adc reg
    int n = readReg(0xfd, readBuf, 2); // Read the config register into readBuf
    if (fDebugLevel > 1)
        printf("%d bytes read\n", n);

    val = ((uint32_t)readBuf[0]) << 8;
    val |= ((uint32_t)readBuf[1]); // (look datasheet page 25)

    return (int32_t)val;
}

TPH BME280::readTPCU()
{
    uint8_t readBuf[8];
    for (int i = 0; i < 8; i++) {
        readBuf[i] = 0;
    }

    TPH val;
    if (!measure()) {
        std::cerr << "error: measurement invalid";
        val.adc_P = INT32_MIN;
        val.adc_T = INT32_MIN;
        val.adc_H = INT32_MIN;
        return val;
    }

    int n = readReg(0xf7, readBuf, 8); // read T, P and H registers;
    if (fDebugLevel > 1)
        printf("%d bytes read\n", n);
    uint32_t adc_P = ((uint32_t)readBuf[0]) << 12;
    adc_P |= ((uint32_t)readBuf[1]) << 4;
    adc_P |= ((uint32_t)readBuf[2]) >> 4;
    uint32_t adc_T = ((uint32_t)readBuf[3]) << 12;
    adc_T |= ((uint32_t)readBuf[4]) << 4;
    adc_T |= ((uint32_t)readBuf[5]) >> 4;
    uint32_t adc_H = ((uint32_t)readBuf[6]) << 8;
    adc_H |= ((uint32_t)readBuf[7]); // (look datasheet page 25)

    val.adc_P = (int32_t)adc_P;
    val.adc_T = (int32_t)adc_T;
    val.adc_H = (int32_t)adc_H;
    return val;
}

bool BME280::softReset()
{
    uint8_t resetWord[1];
    resetWord[0] = 0xb6;
    int val = writeReg(0xe0, resetWord, 1);
    return (val == 1);
}

uint16_t BME280::getCalibParameter(unsigned int param) const
{
    if (param < 18)
        return fCalibParameters[param];
    return 0xffff;
}

void BME280::readCalibParameters()
{
    uint8_t readBuf[32];
    uint8_t readBufPart1[26];
    uint8_t readBufPart2[7];
    // register address first byte eeprom
    int n = readReg(0x88, readBufPart1, 26); // Read the 26 eeprom word values into readBuf
    n = n + readReg(0xe1, readBufPart2, 7); // from two different locations

    for (int i = 0; i < 24; i++) {
        readBuf[i] = readBufPart1[i];
    }
    readBuf[24] = readBufPart1[25];
    for (int i = 0; i < 7; i++) {
        readBuf[i + 25] = readBufPart2[i];
    }
    for (int i = 0; i < 32; i++) {
        if (fDebugLevel > 1) {
            printf("readBuf %d\n", n);
        }
    }
    if (fDebugLevel > 1) {
        printf("%d bytes read\n", n);
    }
    if (fDebugLevel > 1) {
        printf("BME280 eeprom calib data:\n");
    }

    bool ok = true;
    for (int i = 0; i < 12; i++) {
        fCalibParameters[i] = ((uint16_t)readBuf[2 * i]) | (((uint16_t)readBuf[2 * i + 1]) << 8); // 2x 8-Bit ergibt 16-Bit Wort
        if (fCalibParameters[i] == 0 || fCalibParameters[i] == 0xffff)
            ok = false;
        if (fDebugLevel > 1)
            printf("calib%d: %d \n", i, fCalibParameters[i]);
    }
    fCalibParameters[12] = (uint16_t)readBuf[24];
    if (fDebugLevel > 1)
        printf("calib%d: %d \n", 12, fCalibParameters[12]);
    fCalibParameters[13] = ((uint16_t)readBuf[25]) | (((uint16_t)readBuf[26]) << 8);
    if (fDebugLevel > 1)
        printf("calib%d: %d \n", 13, fCalibParameters[13]);
    fCalibParameters[14] = (uint16_t)readBuf[27];
    if (fDebugLevel > 1)
        printf("calib%d: %d \n", 14, fCalibParameters[14]);
    fCalibParameters[15] = (((uint16_t)readBuf[28]) << 4) | (((uint16_t)readBuf[29]) & 0b1111);
    if (fDebugLevel > 1)
        printf("calib%d: %d \n", 15, fCalibParameters[15]);
    fCalibParameters[16] = (((uint16_t)readBuf[29] & 0xf0) >> 4) | (((uint16_t)readBuf[30]) << 4);
    if (fDebugLevel > 1)
        printf("calib%d: %d \n", 16, fCalibParameters[16]);
    fCalibParameters[17] = (uint16_t)readBuf[31];
    if (fDebugLevel > 1)
        printf("calib%d: %d \n", 17, fCalibParameters[17]);

    if (fDebugLevel > 1) {
        if (ok) {
            printf("calib data is valid.\n");
        } else {
            printf("calib data NOT valid!\n");
        }
    }
    fCalibrationValid = ok;
    setDefaultSettings();
}

TPH BME280::getTPHValues()
{
    TPH vals = readTPCU();
    vals.T = getTemperature(vals.adc_T);
    vals.P = getPressure(vals.adc_P);
    vals.H = getHumidity(vals.adc_H);
    return vals;
}

double BME280::getTemperature(int32_t adc_T)
{
    if (!fCalibrationValid)
        return -999.;
    int32_t dig_T1 = (int32_t)fCalibParameters[0];
    int32_t dig_T2 = (int32_t)((int16_t)fCalibParameters[1]);
    int32_t dig_T3 = (int32_t)((int16_t)fCalibParameters[2]);
    int32_t X1 = (((adc_T >> 3) - (dig_T1 << 1)) * dig_T2) >> 11;
    int32_t X2 = (((((adc_T >> 4) - dig_T1) * ((adc_T >> 4) - dig_T1)) >> 12) * dig_T3) >> 14;
    fT_fine = X1 + X2;
    int32_t t = (fT_fine * 5 + 128) >> 8;
    double T = t / 100.0;
    if (fDebugLevel > 1) {
        printf("adc_T=%d\n", adc_T);
        printf("X1=%d\n", X1);
        printf("X2=%d\n", X2);
        printf("t_fine=%d\n", fT_fine);
        printf("temp=%d\n", t);
    }
    return T;
}

double BME280::getHumidity(int32_t adc_H)
{ // please only do this if "getTemperature()" has been executed before
    return getHumidity(adc_H, fT_fine);
}
double BME280::getHumidity(int32_t adc_H, int32_t t_fine)
{
    if (!fCalibrationValid)
        return -999.;
    if (t_fine == 0)
        return -999.;
    int32_t dig_H1 = (int32_t)fCalibParameters[12];
    int32_t dig_H2 = (int32_t)((int16_t)fCalibParameters[13]);
    int32_t dig_H3 = (int32_t)fCalibParameters[14];
    int32_t dig_H4 = (int32_t)((int16_t)fCalibParameters[15]);
    int32_t dig_H5 = (int32_t)((int16_t)fCalibParameters[16]);
    int32_t dig_H6 = (int32_t)((int8_t)((uint8_t)fCalibParameters[17]));
    int32_t X1 = 0;
    X1 = (t_fine - ((int32_t)76800));
    X1 = (((((adc_H << 14) - (dig_H4 << 20) - (dig_H5 * X1)) + ((int32_t)16384)) >> 15) * (((((((X1 * dig_H6) >> 10) * (((X1 * dig_H3) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * dig_H2 + 8192) >> 14));
    X1 = (X1 - (((((X1 >> 15) * (X1 >> 15)) >> 7) * dig_H1) >> 4));
    X1 = (X1 < 0 ? 0 : X1);
    X1 = (X1 > 419430400 ? 419430400 : X1);
    uint32_t h = (uint32_t)(X1 >> 12);
    double H = h / 1024.0;
    if (fDebugLevel > 1) {
        printf("adc_H=%d\n", adc_H);
        printf("X1=%d\n", X1);
        printf("t_fine=%d\n", t_fine);
        printf("Humidity=%u\n", h);
    }
    return H;
}

double BME280::getPressure(signed int adc_P)
{
    return getPressure(adc_P, fT_fine);
}
double BME280::getPressure(signed int adc_P, signed int t_fine)
{
    if (!fCalibrationValid)
        return -999.0;
    if (fT_fine == 0)
        return -999.0;
    uint16_t dig_P1 = fCalibParameters[3];
    int16_t dig_P2 = (int16_t)fCalibParameters[4];
    int16_t dig_P3 = (int16_t)fCalibParameters[5];
    int16_t dig_P4 = (int16_t)fCalibParameters[6];
    int16_t dig_P5 = (int16_t)fCalibParameters[7];
    int16_t dig_P6 = (int16_t)fCalibParameters[8];
    int16_t dig_P7 = (int16_t)fCalibParameters[9];
    int16_t dig_P8 = (int16_t)fCalibParameters[10];
    int16_t dig_P9 = (int16_t)fCalibParameters[11];

    int64_t X1, X2, p;
    X1 = ((int64_t)t_fine) - 128000;
    X2 = X1 * X1 * (int64_t)dig_P6;
    X2 = X2 + ((X1 * (int64_t)dig_P5) << 17);
    X2 = X2 + (((int64_t)dig_P4) << 35);
    X1 = ((X1 * X1 * (int64_t)dig_P3) >> 8) + ((X1 * (int64_t)dig_P2) << 12);
    X1 = (((((int64_t)1) << 47) + X1)) * ((int64_t)dig_P1) >> 33;
    if (X1 == 0) {
        return 0; // avoid exception caused by division by zero
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - X2) * 3125) / X1;
    X1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    X2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + X1 + X2) >> 8) + (((int64_t)dig_P7) << 4);

    double P = ((uint32_t)p) / 256.;
    if (fDebugLevel > 1) {
        printf("adc_P=%d\n", adc_P);
        printf("X1=%lld\n", X1);
        printf("X2=%lld\n", X2);
        printf("p=%lld\n", p);
        printf("P=%.3f\n", P);
    }
    return P;
}
