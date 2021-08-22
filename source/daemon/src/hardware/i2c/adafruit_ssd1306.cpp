#include "hardware/i2c/adafruit_ssd1306.h"
#include <stdint.h>
#include <string.h>

using namespace std;

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
All text above, and the splash screen below must be included in any redistribution

02/18/2013 	Charles-Henri Hallard (http://hallard.me)
Modified for compiling and use on Raspberry ArduiPi Board
LCD size and connection are now passed as arguments on
the command line (no more #define on compilation needed)
ArduiPi project documentation http://hallard.me/arduipi
07/01/2013 	Charles-Henri Hallard
Reduced code size removed the Adafruit Logo (sorry guys)
Buffer for OLED is now dynamic to LCD size
Added support of Seeed OLED 64x64 Display

*********************************************************************/

/*=========================================================================
SSDxxxx Common Displays
-----------------------------------------------------------------------
Common values to all displays
=========================================================================*/

#define SSD_Command_Mode 0x00 /* C0 and DC bit are 0 				 */
#define SSD_Data_Mode 0x40 /* C0 bit is 0 and DC bit is 1 */

#define SSD_Inverse_Display 0xA7

#define SSD_Display_Off 0xAE
#define SSD_Display_On 0xAF

#define SSD_Set_ContrastLevel 0x81

#define SSD_External_Vcc 0x01
#define SSD_Internal_Vcc 0x02

#define SSD_Activate_Scroll 0x2F
#define SSD_Deactivate_Scroll 0x2E

#define Scroll_Left 0x00
#define Scroll_Right 0x01

#define Scroll_2Frames 0x07
#define Scroll_3Frames 0x04
#define Scroll_4Frames 0x05
#define Scroll_5Frames 0x00
#define Scroll_25Frames 0x06
#define Scroll_64Frames 0x01
#define Scroll_128Frames 0x02
#define Scroll_256Frames 0x03

#define VERTICAL_MODE 01
#define PAGE_MODE 01
#define HORIZONTAL_MODE 02

/*=========================================================================
SSD1306 Displays
-----------------------------------------------------------------------
The driver is used in multiple displays (128x64, 128x32, etc.).
=========================================================================*/
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5

#define SSD1306_Normal_Display 0xA6

#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D

// Scrolling #defines
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3
#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A

/*=========================================================================
SSD1308 Displays
-----------------------------------------------------------------------
The driver is used in multiple displays (128x64, 128x32, etc.).
=========================================================================*/
#define SSD1308_Normal_Display 0xA6

/*=========================================================================
SSD1327 Displays
-----------------------------------------------------------------------
The driver is used in Seeed 96x96 display
=========================================================================*/
#define SSD1327_Normal_Display 0xA4

// Low level I2C  Write function
inline void Adafruit_SSD1306::fastI2Cwrite(uint8_t d)
{
    i2cDevice::write(&d, 1);
}
inline void Adafruit_SSD1306::fastI2Cwrite(char* tbuf, uint32_t len)
{
    i2cDevice::write((uint8_t*)tbuf, len);
}

#define _BV(bit) (1 << (bit))
// the most basic function, set a single pixel
void Adafruit_SSD1306::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    uint8_t* p = poledbuff;

    if ((x < 0) || (x >= width()) || (y < 0) || (y >= height()))
        return;

    // check rotation, move pixel around if necessary
    switch (getRotation()) {
    case 1:
        swap(x, y);
        x = WIDTH - x - 1;
        break;

    case 2:
        x = WIDTH - x - 1;
        y = HEIGHT - y - 1;
        break;

    case 3:
        swap(x, y);
        y = HEIGHT - y - 1;
        break;
    }

    // Get where to do the change in the buffer
    p = poledbuff + (x + (y / 8) * ssd1306_lcdwidth);

    // x is which column
    if (color == WHITE)
        *p |= _BV((y % 8));
    else
        *p &= ~_BV((y % 8));
}

// initializer for OLED Type
void Adafruit_SSD1306::select_oled(uint8_t OLED_TYPE)
{
    // Default type
    ssd1306_lcdwidth = 128;
    ssd1306_lcdheight = 64;

    // default OLED are using internal boost VCC converter
    vcc_type = SSD_Internal_Vcc;

    // Oled supported display
    // Setup size and I2C address
    switch (OLED_TYPE) {
    case OLED_ADAFRUIT_I2C_128x32:
        ssd1306_lcdheight = 32;
        break;

    case OLED_ADAFRUIT_I2C_128x64:
        break;

    case OLED_SEEED_I2C_128x64:
        vcc_type = SSD_External_Vcc;
        break;

    case OLED_SEEED_I2C_96x96:
        ssd1306_lcdwidth = 96;
        ssd1306_lcdheight = 96;
        break;

        // houston, we have a problem
    default:
        return;
        break;
    }
}

// initializer for I2C - we only indicate the reset pin and OLED type !
bool Adafruit_SSD1306::init(uint8_t OLED_TYPE, int8_t RST)
{
    rst = RST;

    // Select OLED parameters
    select_oled(OLED_TYPE);

    // De-Allocate memory for OLED buffer if any
    if (poledbuff)
        free(poledbuff);

    // Allocate memory for OLED buffer
    poledbuff = (uint8_t*)malloc((ssd1306_lcdwidth * ssd1306_lcdheight / 8));
    if (!poledbuff)
        return false;

    return (true);
}

void Adafruit_SSD1306::close(void)
{
    // De-Allocate memory for OLED buffer if any
    if (poledbuff)
        free(poledbuff);

    poledbuff = NULL;
}

void Adafruit_SSD1306::begin(void)
{
    uint8_t multiplex;
    uint8_t chargepump;
    uint8_t compins;
    uint8_t contrast;
    uint8_t precharge;

    constructor(ssd1306_lcdwidth, ssd1306_lcdheight);

    // Reset handling, todo

    // depends on OLED type configuration
    if (ssd1306_lcdheight == 32) {
        multiplex = 0x1F;
        compins = 0x02;
        contrast = 0x8F;
    } else {
        multiplex = 0x3F;
        compins = 0x12;
        contrast = (vcc_type == SSD_External_Vcc ? 0x9F : 0xCF);
    }

    if (vcc_type == SSD_External_Vcc) {
        chargepump = 0x10;
        precharge = 0x22;
    } else {
        chargepump = 0x14;
        precharge = 0xF1;
    }

    ssd1306_command(SSD_Display_Off); // 0xAE
    ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV, 0x80); // 0xD5 + the suggested ratio 0x80
    ssd1306_command(SSD1306_SETMULTIPLEX, multiplex);
    ssd1306_command(SSD1306_SETDISPLAYOFFSET, 0x00); // 0xD3 + no offset
    ssd1306_command(SSD1306_SETSTARTLINE | 0x0); // line #0
    ssd1306_command(SSD1306_CHARGEPUMP, chargepump);
    ssd1306_command(SSD1306_MEMORYMODE, 0x00); // 0x20 0x0 act like ks0108
    ssd1306_command(SSD1306_SEGREMAP | 0x1);
    ssd1306_command(SSD1306_COMSCANDEC);
    ssd1306_command(SSD1306_SETCOMPINS, compins); // 0xDA
    ssd1306_command(SSD_Set_ContrastLevel, contrast);
    ssd1306_command(SSD1306_SETPRECHARGE, precharge); // 0xd9
    ssd1306_command(SSD1306_SETVCOMDETECT, 0x40); // 0xDB
    ssd1306_command(SSD1306_DISPLAYALLON_RESUME); // 0xA4
    ssd1306_command(SSD1306_Normal_Display); // 0xA6

    // Reset to default value in case of
    // no reset pin available on OLED
    ssd1306_command(0x21, 0, 127);
    ssd1306_command(0x22, 0, 7);
    stopscroll();

    // Empty uninitialized buffer
    clearDisplay();
    ssd1306_command(SSD_Display_On); //--turn on oled panel
}

void Adafruit_SSD1306::invertDisplay(bool inv)
{
    if (inv)
        ssd1306_command(SSD_Inverse_Display);
    else
        ssd1306_command(SSD1306_Normal_Display);
}

void Adafruit_SSD1306::ssd1306_command(uint8_t c)
{
    char buff[2];

    // Clear D/C to switch to command mode
    buff[0] = SSD_Command_Mode;
    buff[1] = c;

    // Write Data on I2C
    fastI2Cwrite(buff, sizeof(buff));
}

void Adafruit_SSD1306::ssd1306_command(uint8_t c0, uint8_t c1)
{
    char buff[3];
    buff[1] = c0;
    buff[2] = c1;

    // Clear D/C to switch to command mode
    buff[0] = SSD_Command_Mode;

    // Write Data on I2C
    fastI2Cwrite(buff, 3);
}

void Adafruit_SSD1306::ssd1306_command(uint8_t c0, uint8_t c1, uint8_t c2)
{
    char buff[4];

    buff[1] = c0;
    buff[2] = c1;
    buff[3] = c2;

    // Clear D/C to switch to command mode
    buff[0] = SSD_Command_Mode;

    // Write Data on I2C
    fastI2Cwrite(buff, sizeof(buff));
}

// startscrollright
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void Adafruit_SSD1306::startscrollright(uint8_t start, uint8_t stop)
{
    ssd1306_command(SSD1306_RIGHT_HORIZONTAL_SCROLL);
    ssd1306_command(0X00);
    ssd1306_command(start);
    ssd1306_command(0X00);
    ssd1306_command(stop);
    ssd1306_command(0X01);
    ssd1306_command(0XFF);
    ssd1306_command(SSD_Activate_Scroll);
}

// startscrollleft
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void Adafruit_SSD1306::startscrollleft(uint8_t start, uint8_t stop)
{
    ssd1306_command(SSD1306_LEFT_HORIZONTAL_SCROLL);
    ssd1306_command(0X00);
    ssd1306_command(start);
    ssd1306_command(0X00);
    ssd1306_command(stop);
    ssd1306_command(0X01);
    ssd1306_command(0XFF);
    ssd1306_command(SSD_Activate_Scroll);
}

// startscrolldiagright
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void Adafruit_SSD1306::startscrolldiagright(uint8_t start, uint8_t stop)
{
    ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
    ssd1306_command(0X00);
    ssd1306_command(ssd1306_lcdheight);
    ssd1306_command(SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
    ssd1306_command(0X00);
    ssd1306_command(start);
    ssd1306_command(0X00);
    ssd1306_command(stop);
    ssd1306_command(0X01);
    ssd1306_command(SSD_Activate_Scroll);
}

// startscrolldiagleft
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void Adafruit_SSD1306::startscrolldiagleft(uint8_t start, uint8_t stop)
{
    ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
    ssd1306_command(0X00);
    ssd1306_command(ssd1306_lcdheight);
    ssd1306_command(SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
    ssd1306_command(0X00);
    ssd1306_command(start);
    ssd1306_command(0X00);
    ssd1306_command(stop);
    ssd1306_command(0X01);
    ssd1306_command(SSD_Activate_Scroll);
}

void Adafruit_SSD1306::stopscroll(void)
{
    ssd1306_command(SSD_Deactivate_Scroll);
}

void Adafruit_SSD1306::ssd1306_data(uint8_t c)
{
    char buff[2];

    // Setup D/C to switch to data mode
    buff[0] = SSD_Data_Mode;
    buff[1] = c;

    // Write on i2c
    fastI2Cwrite(buff, sizeof(buff));
}

void Adafruit_SSD1306::display(void)
{
    ssd1306_command(SSD1306_SETLOWCOLUMN | 0x0); // low col = 0
    ssd1306_command(SSD1306_SETHIGHCOLUMN | 0x0); // hi col = 0
    ssd1306_command(SSD1306_SETSTARTLINE | 0x0); // line #0

    uint16_t i = 0;

    // pointer to OLED data buffer
    uint8_t* p = poledbuff;

    char buff[17];
    uint8_t x;

    // Setup D/C to switch to data mode
    buff[0] = SSD_Data_Mode;

    // loop trough all OLED buffer and
    // send a bunch of 16 data byte in one xmission
    for (i = 0; i < (ssd1306_lcdwidth * ssd1306_lcdheight / 8); i += 16) {
        for (x = 1; x <= 16; x++)
            buff[x] = *p++;

        fastI2Cwrite(buff, 17);
    }
}

// clear everything (in the buffer)
void Adafruit_SSD1306::clearDisplay(void)
{
    memset(poledbuff, 0, (ssd1306_lcdwidth * ssd1306_lcdheight / 8));
}
