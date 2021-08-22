#include "hardware/i2c/tca9546a.h"
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

void TCA9546A::selectChannel(uint8_t sel) {
    int n;
    ch[0]=sel;
    n = write(ch, 1);
    printf("Written Bytes_SELECT_Channel: %d\n", n);
}
