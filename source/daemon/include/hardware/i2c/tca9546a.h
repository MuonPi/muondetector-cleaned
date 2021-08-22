#ifndef _TCA9546A_H_
#define _TCA9546A_H_

#include "hardware/i2c/i2cdevice.h"

/* TCA9546A  */

class TCA9546A : i2cDevice {
public:
    TCA9546A(): i2cDevice(0x70){}
    TCA9546A(const char* busAddress, uint8_t slaveAddress) : i2cDevice(busAddress,slaveAddress) {}  //init wurde zusätzlich hinzugefügt.
	TCA9546A(uint8_t slaveAddress) : i2cDevice(slaveAddress) {}  //init wurde auch hier zusätzlich hinzugefügt.

    void selectChannel(uint8_t sel);
private:
    enum { CH1=1, CH2=2, CH3=4, CH4=8 };
    uint8_t ch[1];
};

#endif // !_TCA9546A_H_
