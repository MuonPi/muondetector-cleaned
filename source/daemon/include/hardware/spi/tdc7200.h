#ifndef TDC7200_H
#define TDC7200_H

#include <QObject>
#include <QVector>

Q_DECLARE_METATYPE(std::string)

class TDC7200 : public QObject {
    Q_OBJECT
public:
    explicit TDC7200(unsigned int _INTB = 20, QObject* parent = nullptr);

signals:
    void readData(uint8_t reg, unsigned int bytesRead);
    void writeData(uint8_t reg, std::string data);
    void regContentChanged(uint8_t reg, uint32_t data);
    void tdcEvent(double t_diff); // in usecs
    void timeMeas(QVector<double> timings); // in seconds
    void statusUpdated(bool isPresent);

public slots:
    void initialise();
    void onDataReceived(uint8_t reg, std::string data);
    void onDataAvailable(uint8_t pin); // interrupt pin should connect here
    void startMeas();
    void onStatusRequested();

private slots:
    void readReg(uint8_t reg);
    void writeReg(uint8_t reg, uint8_t data);
    void configRegisters();

private:
    void processData();
    bool devicePresent = false;
    uint8_t INTB;
    uint8_t config[2] = { 0x02, 0b00000000 }; // sets meas MODE 2, number of calibration cycles: 20
    // multi-cycle averaging mode: 16 cycles and num_stop: single
    // and then the different masks for start and stop (now all 0 so not written at all)
    double CLOCKperiod = 62.5e-9; // 62.5 ns corresponds to 16 MHz oscillator
    uint32_t regContent1[10];
    uint32_t regContent2[13]; // offset for address of regContent2 is 0x10
    int waitingForDataCounter = -1; // if it is -1 no readout is expected that needs more than one register
    // else it shows the number of already received register contents and if it is 0 all data is read
};

#endif // TDC7200_H
