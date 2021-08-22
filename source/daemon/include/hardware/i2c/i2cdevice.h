#include <fcntl.h> // open
#include <inttypes.h> // uint8_t, etc
#include <iostream>
#include <stdio.h>
#include <string>
#include <sys/ioctl.h> // ioctl
#include <sys/time.h> // for gettimeofday()
#include <unistd.h>
#include <vector>

#ifndef _I2CDEVICE_H_
#define _I2CDEVICE_H_

#define DEFAULT_DEBUG_LEVEL 0

//We define a class named i2cDevices to outsource the hardware dependent program parts. We want to
//access components of integrated curcuits, like the ads1115 or other subdevices via i2c-bus.
//The main aim here is, that the user does not have to be concerned about the c like low level operations
//of I2C access
// First, we define an abstract base class with all low-level i2c acess functions implemented.
// For device specific implementations, classes can inherit this base class
// virtual methods should be reimplemented in the child classes to make sense there, e.g. devicePresent()
class i2cDevice {

public:
    enum MODE { MODE_NONE = 0,
        MODE_NORMAL = 0x01,
        MODE_FORCE = 0x02,
        MODE_UNREACHABLE = 0x04,
        MODE_FAILED = 0x08,
        MODE_LOCKED = 0x10 };

    i2cDevice();
    i2cDevice(const char* busAddress = "/dev/i2c-1");
    i2cDevice(uint8_t slaveAddress);
    i2cDevice(const char* busAddress, uint8_t slaveAddress);
    virtual ~i2cDevice();

    void setAddress(uint8_t address);
    uint8_t getAddress() const { return fAddress; }
    static unsigned int getNrDevices() { return fNrDevices; }
    unsigned int getNrBytesRead() const { return fNrBytesRead; }
    unsigned int getNrBytesWritten() const { return fNrBytesWritten; }
    unsigned int getNrIOErrors() const { return fIOErrors; }
    static unsigned int getGlobalNrBytesRead() { return fGlobalNrBytesRead; }
    static unsigned int getGlobalNrBytesWritten() { return fGlobalNrBytesWritten; }
    static std::vector<i2cDevice*>& getGlobalDeviceList() { return fGlobalDeviceList; }
    virtual bool devicePresent();
    uint8_t getStatus() const { return fMode; }
    void lock(bool locked = true)
    {
        if (locked)
            fMode |= MODE_LOCKED;
        else
            fMode &= ~MODE_LOCKED;
    }

    double getLastTimeInterval() const { return fLastTimeInterval; }

    void setDebugLevel(int level) { fDebugLevel = level; }
    int getDebugLevel() const { return fDebugLevel; }

    void setTitle(const std::string& a_title) { fTitle = a_title; }
    const std::string& getTitle() const { return fTitle; }

    // read nBytes bytes into buffer buf
    // return value:
    // 	the number of bytes actually read if successful
    //	-1 on error
    int read(uint8_t* buf, int nBytes);

    // write nBytes bytes from buffer buf
    // return value:
    // 	the number of bytes actually written if successful
    //	-1 on error
    int write(uint8_t* buf, int nBytes);

    // write nBytes bytes from buffer buf in register reg
    // return value:
    // 	the number of bytes actually written if successful
    //	-1 on error
    // note: first byte of the write sequence is the register address,
    // the following bytes from buf are then written in a sequence
    int writeReg(uint8_t reg, uint8_t* buf, int nBytes);

    // read nBytes bytes into buffer buf from register reg
    // return value:
    // 	the number of bytes actually read if successful
    //	-1 on error
    // note: first writes reg address and after a repeated start
    // reads in a sequence of bytes
    // not all devices support this procedure
    // refer to the device's datasheet
    int readReg(uint8_t reg, uint8_t* buf, int nBytes);

    int8_t readBit(uint8_t regAddr, uint8_t bitNum, uint8_t* data);
    int8_t readBits(uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t* data);
    bool readByte(uint8_t regAddr, uint8_t* data);
    int16_t readBytes(uint8_t regAddr, uint16_t length, uint8_t* data);
    bool writeBit(uint8_t regAddr, uint8_t bitNum, uint8_t data);
    bool writeBits(uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data);
    bool writeByte(uint8_t regAddr, uint8_t data);
    bool writeBytes(uint8_t regAddr, uint16_t length, uint8_t* data);
    bool writeWords(uint8_t regAddr, uint16_t length, uint16_t* data);
    bool writeWord(uint8_t regAddr, uint16_t data);

    void getCapabilities();

protected:
    int fHandle;
    uint8_t fAddress;
    static unsigned int fNrDevices;
    unsigned long int fNrBytesWritten;
    unsigned long int fNrBytesRead;
    static unsigned long int fGlobalNrBytesRead;
    static unsigned long int fGlobalNrBytesWritten;
    double fLastTimeInterval; // the last time measurement's result is stored here
    struct timeval fT1, fT2;
    int fDebugLevel;
    static std::vector<i2cDevice*> fGlobalDeviceList;
    std::string fTitle = "I2C device";
    uint8_t fMode = MODE_NONE;
    unsigned int fIOErrors = 0;

    // fuctions for measuring time intervals
    void startTimer();
    void stopTimer();
};

#endif // _I2CDEVICE_H_
