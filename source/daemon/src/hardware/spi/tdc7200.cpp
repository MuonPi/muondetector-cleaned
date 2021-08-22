#include "hardware/spi/tdc7200.h"
#include <QDebug>

TDC7200::TDC7200(unsigned int _INTB, QObject* parent)
    : QObject(parent)
{
    INTB = (uint8_t)_INTB;
}

void TDC7200::initialise()
{
    devicePresent = false;
    writeReg(0x03, 0x07);
    readReg(0x03);
}

void TDC7200::onDataAvailable(uint8_t pin)
{
    // this means if the INTB is high and there are new measurement results
    // should read all relevant TOF data and after that
    if (pin != INTB) {
        return;
    }
    uint8_t num_stop = config[1] & 0x07;
    if (num_stop > 0b100) {
        num_stop = 0;
    }

    num_stop++;

    waitingForDataCounter = (int)((num_stop)*2 + 3);
    readReg(0x1b); // calibration 1
    readReg(0x1c); // calibration 2
    for (int i = 0; i < num_stop * 2 + 1; i++) {
        // TIME1 and as many TIMEx and CLOCK_COUNTx as num_stop
        // for single num_stop there are TIME1, CLOCK_COUNT1, TIME2
        readReg(i + 0x10);
    }
}

void TDC7200::onStatusRequested()
{
    emit statusUpdated(devicePresent);
}

void TDC7200::configRegisters()
{
    for (int i = sizeof(config) - 1; i > 0; i--) {
        writeReg(i, config[i]);
    }
}

void TDC7200::startMeas()
{
    if (!devicePresent) {
        initialise();
        return;
    }
    writeReg(0, config[0] | 0x01); // the least significant bit starts the measurement
}

void TDC7200::onDataReceived(uint8_t reg, std::string data)
{
    if (devicePresent == false) {
        if (reg == 0x03 && data.size() == 1 and data[0] == (char)0x07) {
            devicePresent = true; // device is present, do configuration
            emit statusUpdated(true);
            configRegisters();
            return;
        } else {
            emit statusUpdated(false);
            qDebug() << "TDC7200 not present";
            return;
        }
    }
    if (reg < 10) {
        if (data.size() != 1) {
            qDebug() << "data size returned does not match the register size";
            return;
        }
    } else if (data.size() != 3) {
        qDebug() << "data size returned does not match the register size";
        return;
    } else if (reg > 0x1c) {
        qDebug() << "returned register address out of scope";
        return;
    }
    uint32_t regContent = 0;
    if (reg < 10) {
        regContent = (uint32_t)data[0];
        regContent1[reg] = (uint8_t)data[0];
    } else {
        regContent = data[0];
        regContent <<= 8;
        regContent |= data[1];
        regContent <<= 8;
        regContent |= data[2];
        regContent2[reg - 0x10] = regContent;
        if (waitingForDataCounter != -1) {
            waitingForDataCounter--;
        }
    }
    emit regContentChanged(reg, regContent);
    if (waitingForDataCounter == 0) {
        waitingForDataCounter = -1;
        processData();
    }
}

void TDC7200::processData()
{
    uint8_t num_stop = config[1] & 0x07;
    if (num_stop > 0b100) {
        num_stop = 0;
    }
    num_stop += 1;
    uint8_t meas_mode = (config[0] & 0x2) >> 1;
    uint32_t CALIBRATION1 = regContent2[0x1b - 0x10];
    uint32_t CALIBRATION2 = regContent2[0x1c - 0x10];
    uint8_t cal2_periods_setting = (config[1] & 0b11000000) >> 6;
    uint8_t CALIBRATION2_PERIODS = 0;
    switch (cal2_periods_setting) {
    case 0:
        CALIBRATION2_PERIODS = 2;
        break;
    case 1:
        CALIBRATION2_PERIODS = 10;
        break;
    case 2:
        CALIBRATION2_PERIODS = 20;
        break;
    case 3:
        CALIBRATION2_PERIODS = 40;
        break;
    default:
        break;
    }

    QVector<double> timings;
    double calCount = ((double)CALIBRATION2 - (double)CALIBRATION1) / ((double)CALIBRATION2_PERIODS - 1);
    double normLSB = (double)CLOCKperiod / calCount;
    double TIME1 = (double)regContent2[0];
    for (int i = 0; i < num_stop; i++) {
        if (meas_mode == 0) { // mode 1
            double TIMEx = (double)regContent2[i * 2];
            double TOF = TIMEx / normLSB;
            timings.push_back(TOF);
        }
        if (meas_mode == 1) { // mode 2
            double TIMEn1 = (double)regContent2[i * 2 + 2];
            double TOF = normLSB * (TIME1 - TIMEn1) + ((double)regContent2[i + 1]) * CLOCKperiod;
            timings.push_back(TOF);
        }
    }
    emit timeMeas(timings);
    emit tdcEvent(timings.at(0) * 1e6);
    startMeas();
}

void TDC7200::writeReg(uint8_t reg, uint8_t data)
{
    uint8_t command = reg & 0x3f;
    command |= 0x40; // bit 6 is 1 => write and bit 7 is 0 for writing only one byte
    std::string dataString;
    dataString += (char)data;
    emit writeData(command, dataString);
}

void TDC7200::readReg(uint8_t reg)
{
    unsigned int bytesRead = 0;
    if (reg < 10) {
        bytesRead = 1;
    } else {
        bytesRead = 3;
    }
    uint8_t command = reg & 0x3f;
    emit readData(command, bytesRead);
}
