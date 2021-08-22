#include "hardware/i2c/ads1115.h"
/*
* ADS1115 4ch 16 bit ADC
*/
const double ADS1115::PGAGAINS[6] = { 6.144, 4.096, 2.048, 1.024, 0.512, 0.256 };

int16_t ADS1115::readADC(unsigned int channel)
{
    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device
    uint8_t readBuf[2]; // 2 byte buffer to store the data read from the I2C device
    int16_t val; // Stores the 16 bit value of our ADC conversion
        //  uint8_t fDataRate = 0x07; // 860 SPS
        //  uint8_t fDataRate = 0x01; // 16 SPS
    uint8_t fDataRate = fRate & 0x07;

    startTimer();

    // These three bytes are written to the ADS1115 to set the config register and start a conversion
    writeBuf[0] = 0x01; // This sets the pointer register so that the following two bytes write to the config register
    writeBuf[1] = 0x80; // OS bit
    if (!fDiffMode)
        writeBuf[1] |= 0x40; // single ended mode channels
    writeBuf[1] |= (channel & 0x03) << 4; // channel select
    writeBuf[1] |= 0x01; // single shot mode
    writeBuf[1] |= ((uint8_t)fPga[channel]) << 1; // PGA gain select

    // This sets the 8 LSBs of the config register (bits 7-0)
    writeBuf[2] = 0x00; // enable ALERT/RDY pin
    writeBuf[2] |= ((uint8_t)fDataRate) << 5;

    // Initialize the buffer used to read data from the ADS1115 to 0
    readBuf[0] = 0;
    readBuf[1] = 0;

    // Write writeBuf to the ADS1115, the 3 specifies the number of bytes we are writing,
    // this begins a single conversion
    write(writeBuf, 3);

    // Wait for the conversion to complete, this requires bit 15 to change from 0->1
    int nloops = 0;
    while ((readBuf[0] & 0x80) == 0 && nloops * fReadWaitDelay / 1000 < 1000) // readBuf[0] contains 8 MSBs of config register, AND with 10000000 to select bit 15
    {
        usleep(fReadWaitDelay);
        read(readBuf, 2); // Read the config register into readBuf
        nloops++;
    }
    if (nloops * fReadWaitDelay / 1000 >= 1000) {
        if (fDebugLevel > 1)
            printf("timeout!\n");
        return INT16_MIN;
    }
    if (fDebugLevel > 2)
        printf(" nr of busy adc loops: %d \n", nloops);
    if (nloops > 1) {
        fReadWaitDelay += (nloops - 1) * fReadWaitDelay / 10;
        if (fDebugLevel > 1) {
            printf(" read wait delay: %6.2f ms\n", fReadWaitDelay / 1000.);
        }
    }

    // Set pointer register to 0 to read from the conversion register
    readReg(0x00, readBuf, 2); // Read the contents of the conversion register into readBuf

    val = readBuf[0] << 8 | readBuf[1]; // Combine the two bytes of readBuf into a single 16 bit result
    fLastADCValue = val;

    stopTimer();
    fLastConvTime = fLastTimeInterval;

    return val;
}

bool ADS1115::setLowThreshold(int16_t thr)
{
    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device
    uint8_t readBuf[2]; // 2 byte buffer to store the data read from the I2C device
    startTimer();

    // These three bytes are written to the ADS1115 to set the Lo_thresh register
    writeBuf[0] = 0x02; // This sets the pointer register to Lo_thresh register
    writeBuf[1] = (thr & 0xff00) >> 8;
    writeBuf[2] |= (thr & 0x00ff);

    // Initialize the buffer used to read data from the ADS1115 to 0
    readBuf[0] = 0;
    readBuf[1] = 0;

    // Write writeBuf to the ADS1115, the 3 specifies the number of bytes we are writing,
    // this sets the Lo_thresh register
    int n = write(writeBuf, 3);
    if (n != 3)
        return false;
    n = read(readBuf, 2); // Read the same register into readBuf for verification
    if (n != 2)
        return false;

    if ((readBuf[0] != writeBuf[1]) || (readBuf[1] != writeBuf[2]))
        return false;
    stopTimer();
    return true;
}

bool ADS1115::setHighThreshold(int16_t thr)
{
    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device
    uint8_t readBuf[2]; // 2 byte buffer to store the data read from the I2C device
    startTimer();

    // These three bytes are written to the ADS1115 to set the Hi_thresh register
    writeBuf[0] = 0x03; // This sets the pointer register to Hi_thresh register
    writeBuf[1] = (thr & 0xff00) >> 8;
    writeBuf[2] |= (thr & 0x00ff);

    // Initialize the buffer used to read data from the ADS1115 to 0
    readBuf[0] = 0;
    readBuf[1] = 0;

    // Write writeBuf to the ADS1115, the 3 specifies the number of bytes we are writing,
    // this sets the Hi_thresh register
    int n = write(writeBuf, 3);
    if (n != 3)
        return false;

    n = read(readBuf, 2); // Read the same register into readBuf for verification
    if (n != 2)
        return false;

    if ((readBuf[0] != writeBuf[1]) || (readBuf[1] != writeBuf[2]))
        return false;
    stopTimer();
    return true;
}

bool ADS1115::setDataReadyPinMode()
{
    // c.f. datasheet, par. 9.3.8, p. 19
    // set MSB of Lo_thresh reg to 0
    // set MSB of Hi_thresh reg to 1
    // set COMP_QUE[1:0] to any value other than '11' (default value)
    bool ok = setLowThreshold(0b00000000);
    ok = ok && setHighThreshold(0b11111111);
    return ok;
}

bool ADS1115::devicePresent()
{
    uint8_t buf[2];
    return (read(buf, 2) == 2); // Read the currently selected register into readBuf
}

double ADS1115::readVoltage(unsigned int channel)
{
    double voltage = 0.;
    readVoltage(channel, voltage);
    return voltage;
}

void ADS1115::readVoltage(unsigned int channel, double& voltage)
{
    int16_t adc = 0;
    readVoltage(channel, adc, voltage);
}

void ADS1115::readVoltage(unsigned int channel, int16_t& adc, double& voltage)
{
    adc = readADC(channel);
    voltage = PGAGAINS[fPga[channel]] * adc / 32767.0;

    if (fAGC) {
        int eadc = abs(adc);
        if (eadc > 0.8 * 32767 && (unsigned int)fPga[channel] > 0) {
            fPga[channel] = CFG_PGA((unsigned int)fPga[channel] - 1);
            if (fDebugLevel > 1)
                printf("ADC input high...setting PGA to level %d\n", fPga[channel]);
        } else if (eadc < 0.2 * 32767 && (unsigned int)fPga[channel] < 5) {
            fPga[channel] = CFG_PGA((unsigned int)fPga[channel] + 1);
            if (fDebugLevel > 1)
                printf("ADC input low...setting PGA to level %d\n", fPga[channel]);
        }
    }
    fLastVoltage = voltage;
    return;
}
