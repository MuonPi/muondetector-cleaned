#ifndef _BME280_H_
#define _BME280_H_

#include "hardware/i2c/i2cdevice.h"

/* BME280  */
// struct to store temperature, pressure and humidity data in different ways
struct TPH {
    uint32_t adc_T;
    uint32_t adc_P;
    uint32_t adc_H;
    double T, P, H;
};
class BME280 : public i2cDevice { // t_max = 112.8 ms for all three measurements at max oversampling
public:
    BME280()
        : i2cDevice(0x76)
    {
        init();
    }
    BME280(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        init();
    }
    BME280(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        init();
    }

    bool init();
    bool status();
    bool measure();
    uint8_t readConfig();
    uint8_t read_CtrlMeasReg();
    bool writeConfig(uint8_t config);
    bool write_CtrlMeasReg(uint8_t config);
    bool setMode(uint8_t mode); // 3 bits: "1=sleep", "2=single shot", "3=interval"
    bool setTSamplingMode(uint8_t mode);
    bool setPSamplingMode(uint8_t mode);
    bool setHSamplingMode(uint8_t mode);
    bool setDefaultSettings();
    bool softReset();
    void readCalibParameters();
    inline bool isCalibValid() const { return fCalibrationValid; }
    uint16_t getCalibParameter(unsigned int param) const;
    int32_t readUT();
    int32_t readUP();
    int32_t readUH();
    TPH readTPCU();
    TPH getTPHValues();
    double getTemperature(int32_t adc_T);
    double getPressure(signed int adc_P);
    double getPressure(signed int adc_P, signed int t_fine);
    double getHumidity(int32_t adc_H);
    double getHumidity(int32_t adc_H, int32_t t_fine);

private:
    int32_t fT_fine = 0;
    unsigned int fLastConvTime;
    bool fCalibrationValid;
    uint16_t fCalibParameters[18]; // 18x 16-Bit words in 36 8-Bit registers
};

#endif // !_BME280_H_
