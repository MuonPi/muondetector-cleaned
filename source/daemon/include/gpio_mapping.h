#ifndef GPIO_MAPPING_H
#define GPIO_MAPPING_H
#include <map>
#include "gpio_pin_definitions.h"


#define MAX_HW_VER 3


// Mapping between signals of the MuonPi hardware interface and the actual GPIO pins of the RaspberryPi
// ATTENTION:
// All pins are numbered according to the BCM designation

static const std::map<GPIO_SIGNAL, unsigned int> GPIO_PINMAP_VERSIONS[MAX_HW_VER+1] = {
		{
			/* Pin mapping, HW Version 0, proxy to be never used nor initializing something */
		} ,
		{
			/* Pin mapping, HW Version 1 */
            { UBIAS_EN , 4 },
            { PREAMP_1 , 20 },
            { PREAMP_2 , 21 },
			{ EVT_AND , 5 },
			{ EVT_XOR , 6 },
            { GAIN_HL , 17 },
			{ ADC_READY , 23 },
			{ TIMEPULSE , 18 },
            { STATUS1 , 13 },
            { STATUS2 , 19 },
            { STATUS3 , 26 },
            { TDC_INTB, 24 },
            { TDC_STATUS, 25 },
			{ EXT_TRIGGER, 16 }
		} ,
		{
			/* Pin mapping, HW Version 2 */
            { UBIAS_EN , 26 },
            { PREAMP_1 , 4 },
            { PREAMP_2 , 17 },
            { EVT_AND , 22 },
            { EVT_XOR , 27 },
            { GAIN_HL , 6 },
            { ADC_READY , 12 },
			{ TIMEPULSE , 18 },
			{ TIME_MEAS_OUT , 5 },
            { STATUS1 , 13 },
            { STATUS2 , 19 },
            { PREAMP_FAULT , 23 },
            { TDC_INTB, 20 },
            { TDC_STATUS, 21 },
			{ EXT_TRIGGER, 16 }
		} ,
		{
			/* Pin mapping, HW Version 3 */
            { UBIAS_EN , 26 },
            { PREAMP_1 , 4 },
            { PREAMP_2 , 17 },
            { EVT_AND , 22 },
            { EVT_XOR , 27 },
            { GAIN_HL , 6 },
            { ADC_READY , 12 },
			{ TIMEPULSE , 18 },
			{ TIME_MEAS_OUT , 5 },
            { STATUS1 , 13 },
            { STATUS2 , 19 },
            { PREAMP_FAULT , 23 },
            { TDC_INTB, 20 },
            { TDC_STATUS, 21 },
			{ EXT_TRIGGER, 16 },
			{ IN_POL1, 24 },
			{ IN_POL2, 25 }
		}
	};


extern std::map<GPIO_SIGNAL, unsigned int> GPIO_PINMAP;

inline GPIO_SIGNAL bcmToGpioSignal(unsigned int bcmGpioNumber) {
	std::map<GPIO_SIGNAL, unsigned int>::const_iterator i = GPIO_PINMAP.cbegin();
	while (i != GPIO_PINMAP.cend() && i->second!=bcmGpioNumber)	++i;
	if (i==GPIO_PINMAP.cend()) return UNDEFINED_SIGNAL;
	return i->first;
}

#endif //GPIO_MAPPING_H
