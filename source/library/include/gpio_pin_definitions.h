#ifndef GPIO_SIGNAL_DEFINITIONS_H
#define GPIO_SIGNAL_DEFINITIONS_H

#include "muondetector_shared_global.h"

#include <QMap>
#include <QVector>
#include <QString>
#include <map>

// Define the signals of the hardware interface to the MuonPi HAT
// Note: The pin definitions are enum constants and have nothing to do with the actual pin numbers
// of the RPi GPIO header. To be independent of the specific hardware implementation,
// the pin numbers for these signals are defined in gpio_pin_mapping.h on the daemon side through the static map GPIO_PINMAP

enum GPIO_SIGNAL {	UBIAS_EN, 
					PREAMP_1, PREAMP_2, 
					EVT_AND, EVT_XOR, 
					GAIN_HL, ADC_READY, 
					TIMEPULSE, 
					TIME_MEAS_OUT, 
					STATUS1, STATUS2, STATUS3, 
                    PREAMP_FAULT, EXT_TRIGGER,
                    TDC_INTB,
                    TDC_STATUS,
					IN_POL1, IN_POL2,
					UNDEFINED_SIGNAL=255
				};

enum SIGNAL_DIRECTION { DIR_UNDEFINED, DIR_IN, DIR_OUT, DIR_IO };
//enum SIGNAL_POLARITY { POL_UNDEFINED, POL_POSITIVE, POL_NEGATIVE, POL_ANY };

struct GpioSignalDescriptor {
	QString name;
	SIGNAL_DIRECTION direction;
//	SIGNAL_POLARITY polarity;
};

static const QMap<GPIO_SIGNAL, GpioSignalDescriptor> GPIO_SIGNAL_MAP =
	{	{ UBIAS_EN,			{ "UBIAS_EN", DIR_OUT } },
		{ PREAMP_1,			{ "PREAMP_1", DIR_OUT } },
		{ PREAMP_2,			{ "PREAMP_2", DIR_OUT } },
		{ EVT_AND,			{ "EVT_AND", DIR_IN }  },
		{ EVT_XOR,			{ "EVT_XOR", DIR_IN }  },
		{ GAIN_HL,			{ "GAIN_HL", DIR_OUT } },
		{ ADC_READY,		{ "ADC_READY", DIR_IN } },
		{ TIMEPULSE,		{ "TIMEPULSE", DIR_IN }},
		{ TIME_MEAS_OUT,	{ "TIME_MEAS_OUT", DIR_IN } },
		{ STATUS1,			{ "STATUS1", DIR_OUT } },
		{ STATUS2,			{ "STATUS2", DIR_OUT } },
		{ STATUS3,			{ "STATUS3", DIR_OUT } },
		{ PREAMP_FAULT,		{ "PREAMP_FAULT", DIR_IN } },
        { EXT_TRIGGER,		{ "EXT_TRIGGER", DIR_IN } },
        { TDC_INTB, 		{ "TDC_INTB", DIR_IN } },
        { TDC_STATUS, 		{ "TDC_STATUS", DIR_OUT} },
		{ IN_POL1, 			{ "IN_POL1", DIR_OUT} },
		{ IN_POL2, 			{ "IN_POL2", DIR_OUT} },
		{ UNDEFINED_SIGNAL, { "UNDEFINED_SIGNAL", DIR_UNDEFINED} }
	};


static const QVector<QString> TIMING_MUX_SIGNAL_NAMES = 
	{	
		"AND", "XOR", "DISCR1", "DISCR2", "N/A", "TIMEPULSE", "N/A", "EXT"	
	};

enum class TIMING_MUX_SELECTION : uint8_t {
	AND 	= 0,
	XOR 	= 1,
	DISCR1 	= 2,
	DISCR2	= 3,
	TIMEPULSE = 5,
	EXT		= 7,
	UNDEFINED = 255
};
	
#endif // GPIO_SIGNAL_DEFINITIONS_H
