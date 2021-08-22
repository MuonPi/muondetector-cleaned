#ifndef _ADS1115_H_
#define _ADS1115_H_

#include "hardware/i2c/i2cdevice.h"

// ADC ADS1x13/4/5 sampling readout delay
#define READ_WAIT_DELAY_INIT 10

/* ADS1115: 4(2) ch, 16 bit ADC  */

class ADS1115 : public i2cDevice {
public:
    enum CFG_CHANNEL { CH0 = 0,
        CH1,
        CH2,
        CH3 };
    enum CFG_DIFF_CHANNEL { CH0_1 = 0,
        CH0_3,
        CH1_3,
        CH2_3 };
    enum CFG_PGA { PGA6V = 0,
        PGA4V = 1,
        PGA2V = 2,
        PGA1V = 3,
        PGA512MV = 4,
        PGA256MV = 5 };
    static const double PGAGAINS[6];

    ADS1115()
        : i2cDevice("/dev/i2c-1", 0x48)
    {
        init();
    }
    ADS1115(uint8_t slaveAddress)
        : i2cDevice(slaveAddress)
    {
        init();
    }
    ADS1115(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        init();
    }
    ADS1115(const char* busAddress, uint8_t slaveAddress, CFG_PGA pga)
        : i2cDevice(busAddress, slaveAddress)
    {
        init();
        setPga(pga);
    }

    void setPga(CFG_PGA pga) { fPga[0] = fPga[1] = fPga[2] = fPga[3] = pga; }
    void setPga(unsigned int pga) { setPga((CFG_PGA)pga); }
    void setPga(uint8_t channel, CFG_PGA pga)
    {
        if (channel > 3)
            return;
        fPga[channel] = pga;
    }
    void setPga(uint8_t channel, uint8_t pga) { setPga(channel, (CFG_PGA)pga); }
    CFG_PGA getPga(int ch) const { return fPga[ch]; }
    void setAGC(bool state) { fAGC = state; }
    bool getAGC() const { return fAGC; }
    void setRate(unsigned int rate) { fRate = rate & 0x07; }
    unsigned int getRate() const { return fRate; }
    bool setLowThreshold(int16_t thr);
    bool setHighThreshold(int16_t thr);
    int16_t readADC(unsigned int channel);
    double readVoltage(unsigned int channel);
    void readVoltage(unsigned int channel, double& voltage);
    void readVoltage(unsigned int channel, int16_t& adc, double& voltage);
    bool devicePresent();
    void setDiffMode(bool mode) { fDiffMode = mode; }
    bool setDataReadyPinMode();
    unsigned int getReadWaitDelay() const { return fReadWaitDelay; }
    double getLastConvTime() const { return fLastConvTime; }

protected:
    CFG_PGA fPga[4];
    unsigned int fRate;
    double fLastConvTime;
    unsigned int fLastADCValue;
    double fLastVoltage;
    unsigned int fReadWaitDelay; // conversion wait time in us
    bool fAGC; // software agc which switches over to a better pga setting if voltage too low/high
    bool fDiffMode = false; // measure differential input signals (true) or single ended (false=default)

    inline virtual void init()
    {
        fPga[0] = fPga[1] = fPga[2] = fPga[3] = PGA4V;
        fReadWaitDelay = READ_WAIT_DELAY_INIT;
        fRate = 0x00; // RATE8
        fAGC = false;
        fTitle = "ADS1115";
    }
};

#endif // !_ADS1115_H_
