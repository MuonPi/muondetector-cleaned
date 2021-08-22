#include "hardware/i2c/pca9536.h"
#include <stdint.h>

/*
* PCA9536 4 pin I/O expander
*/

bool PCA9536::setOutputPorts(uint8_t portMask)
{
    unsigned char data = ~portMask;
    startTimer();
    if (1 != writeReg(CONFIG_REG, &data, 1)) {
        return false;
    }
    stopTimer();
    return true;
}

bool PCA9536::setOutputState(uint8_t portMask)
{
    startTimer();
    if (1 != writeReg(OUTPUT_REG, &portMask, 1)) {
        return false;
    }
    stopTimer();
    return true;
}

uint8_t PCA9536::getInputState()
{
    uint8_t inport = 0x00;
    startTimer();
    readReg(INPUT_REG, &inport, 1);
    stopTimer();
    return inport & 0x0f;
}

bool PCA9536::devicePresent()
{
    uint8_t inport = 0x00;
    // read input port
    return (1 == readReg(INPUT_REG, &inport, 1));
}
