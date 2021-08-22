/*
	Every address is defined here
*/

#define ADC1_ADDR 0x4a
#define ADC2_ADDR 0x48
#define LM75_ADDR 0x4c
#define POTI1_ADDR 0x28
#define POTI2_ADDR 0x29
#define POTI3_ADDR 0x2a
#define POTI4_ADDR 0x2c
#define EEP_ADDR 0x50

#define VMEAS_FACTOR (0.165 + 22.0) / 0.165
#define IMEAS_FACTOR 1. / (4.7e-3)
#define VMEAS_OFFSET 0.0
#define IMEAS_OFFSET -22.5
#define VTEMP_OFFSET 0.0
#define VTEMP_SLOPE 0.0
#define ITEMP_OFFSET 0.0
#define ITEMP_SLOPE 0.1205
