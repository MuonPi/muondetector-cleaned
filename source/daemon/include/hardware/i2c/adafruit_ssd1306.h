#ifndef _ADAFRUIT_SSD1306_H_
#define _ADAFRUIT_SSD1306_H_

#include "hardware/i2c/i2cdevice.h"

#include "hardware/i2c/Adafruit_GFX.h"
// OLED defines
#define OLED_I2C_RESET RPI_V2_GPIO_P1_22 /* GPIO 25 pin 12  */
// Oled supported display
#define OLED_ADAFRUIT_SPI_128x32 0
#define OLED_ADAFRUIT_SPI_128x64 1
#define OLED_ADAFRUIT_I2C_128x32 2
#define OLED_ADAFRUIT_I2C_128x64 3
#define OLED_SEEED_I2C_128x64 4
#define OLED_SEEED_I2C_96x96 5
#define OLED_LAST_OLED 6 /* always last type, used in code to end array */

/*********************************************************************
This is a library for our Monochrome OLEDs based on SSD1306 drivers

Pick one up today in the adafruit shop!
------> http://www.adafruit.com/category/63_98

These displays use SPI to communicate, 4 or 5 pins are required to
interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen must be included in any redistribution

02/18/2013 	Charles-Henri Hallard (http://hallard.me)
Modified for compiling and use on Raspberry ArduiPi Board
LCD size and connection are now passed as arguments on
the command line (no more #define on compilation needed)
ArduiPi project documentation http://hallard.me/arduipi

*********************************************************************/
class Adafruit_SSD1306 : public i2cDevice, public Adafruit_GFX {
public:
    enum { BLACK = 0,
        WHITE = 1 };

    Adafruit_SSD1306()
        : i2cDevice(0x3c)
    {
        fTitle = "SSD1306 OLED";
        init(OLED_ADAFRUIT_I2C_128x64, -1);
    }
    Adafruit_SSD1306(const char* busAddress, uint8_t slaveAddress)
        : i2cDevice(busAddress, slaveAddress)
    {
        fTitle = "SSD1306 OLED";
        init(OLED_ADAFRUIT_I2C_128x64, -1);
    }
    Adafruit_SSD1306(uint8_t slaveAddress, uint8_t OLED_TYPE = OLED_ADAFRUIT_I2C_128x64, int8_t rst_pin = -1)
        : i2cDevice(slaveAddress)
    {
        fTitle = "SSD1306 OLED";
        init(OLED_TYPE, rst_pin);
    }

    ~Adafruit_SSD1306() { close(); }

    // I2C Init
    bool init(uint8_t OLED_TYPE, int8_t RST);

    void select_oled(uint8_t OLED_TYPE);

    void begin(void);
    void close(void);

    void ssd1306_command(uint8_t c);
    void ssd1306_command(uint8_t c0, uint8_t c1);
    void ssd1306_command(uint8_t c0, uint8_t c1, uint8_t c2);
    void ssd1306_data(uint8_t c);

    void clearDisplay(void);
    void invertDisplay(bool inv);
    void display();

    void startscrollright(uint8_t start, uint8_t stop);
    void startscrollleft(uint8_t start, uint8_t stop);

    void startscrolldiagright(uint8_t start, uint8_t stop);
    void startscrolldiagleft(uint8_t start, uint8_t stop);
    void stopscroll(void);

    void drawPixel(int16_t x, int16_t y, uint16_t color);

private:
    uint8_t* poledbuff = nullptr; // Pointer to OLED data buffer in memory
    int8_t rst;
    int16_t ssd1306_lcdwidth, ssd1306_lcdheight;
    uint8_t vcc_type;

    void fastI2Cwrite(uint8_t c);
    void fastI2Cwrite(char* tbuf, uint32_t len);
};
#endif // !_ADAFRUIT_SSD1306_H_
