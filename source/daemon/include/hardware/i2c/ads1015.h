#ifndef _ADS1015_H_
#define _ADS1015_H_
#include "hardware/i2c/ads1115.h"

/* ADS1015: 4(2) ch, 12 bit ADC  */

class ADS1015 : public ADS1115 {
public:
    using ADS1115::ADS1115;

protected:
    inline void init()
    {
        ADS1115::init();
        fTitle = "ADS1015";
    }
};

#endif // !_ADS1015_H_
