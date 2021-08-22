#ifndef DAEMON_H
#define DAEMON_H

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QSocketNotifier>
#include <QTcpServer>
#include <QTimer>
#include <QVariant>
#include <time.h>

// clang-format off
#include "qtserialublox.h"
#include "utility/filehandler.h"
#include "calibration.h"
// clang-format on

#include "utility/custom_io_operators.h"
#include "histogram.h"
#include "hardware/i2cdevices.h"
#include "logengine.h"
#include "logparameter.h"
#include "mqtthandler.h"
#include "pigpiodhandler.h"
#include "tcpconnection.h"
#include "hardware/spidevices.h"

// for sig handling:
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

struct CalibStruct;
struct UbxTimeMarkStruct;
enum GPIO_PIN;

class Property {
public:
    Property() = default;

    template <typename T>
    Property(const T& val)
        : name("")
        , unit("")
    {
        typeId = qMetaTypeId<T>();
        if (typeId == QMetaType::UnknownType) {
            typeId = qRegisterMetaType<T>();
        }
        value = QVariant(val);
        updated = true;
    }

    template <typename T>
    Property(const QString& a_name, const T& val, const QString& a_unit = "")
        : name(a_name)
        , unit(a_unit)
    {
        typeId = qMetaTypeId<T>();
        if (typeId == QMetaType::UnknownType) {
            typeId = qRegisterMetaType<T>();
        }
        value = QVariant(val);
        updated = true;
    }

    Property(const Property& prop) = default;
    Property& operator=(const Property& prop)
    {
        name = prop.name;
        unit = prop.unit;
        value = prop.value;
        typeId = prop.typeId;
        updated = prop.updated;
        return *this;
    }

    Property& operator=(const QVariant& val)
    {
        value = val;
        //lastUpdate = std::chrono::system_clock::now();
        updated = true;
        return *this;
    }

    const QVariant& operator()()
    {
        updated = false;
        return value;
    }

    bool isUpdated() const { return updated; }
    int type() const { return typeId; }

    QString name = "";
    QString unit = "";

private:
    QVariant value;
    bool updated = false;
    int typeId = 0;
};

struct RateScanInfo {
    uint8_t origPcaMask = 0;
    GPIO_PIN origEventTrigger = GPIO_PIN::UNDEFINED_PIN;
    uint16_t lastEvtCounter = 0;
    uint8_t thrChannel = 0;
    float origThr = 3.3;
    float thrIncrement = 0.1;
    float minThr = 0.05;
    float maxThr = 3.3;
    float currentThr = 0.;
    uint16_t nrLoops = 0;
    uint16_t currentLoop = 0;
};

struct RateScan {
    uint8_t origPcaMask = 0;
    GPIO_PIN origEventTrigger = GPIO_PIN::UNDEFINED_PIN;
    float origScanPar = 3.3;
    double minScanPar = 0.;
    double maxScanPar = 1.;
    double currentScanPar = 0.;
    double scanParIncrement = 0.;
    uint32_t currentCounts = 0;
    double currentTimeInterval = 0.;
    double maxTimeInterval = 1.;
    uint16_t nrLoops = 0;
    uint16_t currentLoop = 0;
    QMap<double, double> scanMap;
};

class Daemon : public QTcpServer {
    Q_OBJECT

public:
    struct configuration {
        QString username;
        QString password;
        QString gpsdevname { "" };
        int verbose { 0 };
        quint8 pcaPortMask { 0 };
        std::array<float, 2> dacThresh { -1.0F, -1.0F };
        float biasVoltage { -1.0F };
        bool bias_ON { false };
        bool dumpRaw;
        int baudrate { 9600 };
        bool configGnss { false };
        unsigned int eventTrigger { EVT_XOR };
        QString peerAddress { "" };
        quint16 peerPort { 0 };
        QString serverAddress { "" };
        quint16 serverPort { 0 };
        bool showout { false };
        bool showin { false };
        std::array<bool, 2> preamp { false, false };
        bool gain { false };
        QString station_ID { "0" };
        std::array<bool, 2> polarity { true, true };
        int maxGeohashLength { MuonPi::Settings::log.max_geohash_length };
        bool storeLocal { false };
    };

    Daemon(configuration cfg, QObject* parent = nullptr);

    ~Daemon() override;
    void configGps();
    void configGpsForVersion();
    void loop();
    static void hupSignalHandler(int);
    static void termSignalHandler(int);
    static void intSignalHandler(int);

public slots:
    // Qt signal handlers.
    void handleSigHup();
    void handleSigTerm();
    void handleSigInt();
    // others
    void connectToPigpiod();
    void connectToGps();
    void displaySocketError(int socketError, QString message);
    void displayError(QString message);
    void toConsole(const QString& data);
    void gpsToConsole(const QString& data);
    void onMadeConnection(QString remotePeerAddress, quint16 remotePeerPort, QString localAddress, quint16 localPort);
    void onStoppedConnection(QString remotePeerAddress, quint16 remotePeerPort, QString localAddress, quint16 localPort, quint32 timeoutTime, quint32 connectionDuration);
    void UBXReceivedAckNak(uint16_t ackedMsgID, uint16_t ackedCfgMsgID);
    void UBXReceivedMsgRateCfg(uint16_t msgID, uint8_t rate);
    void gpsConnectionError();
    void gpsPropertyUpdatedInt32(int32_t data, std::chrono::duration<double> updateAge, char propertyName);
    void gpsPropertyUpdatedUint32(uint32_t data, std::chrono::duration<double> updateAge, char propertyName);
    void gpsPropertyUpdatedUint8(uint8_t data, std::chrono::duration<double> updateAge, char propertyName);
    void onUBXReceivedTxBuf(uint8_t txUsage, uint8_t txPeakUsage);
    void onUBXReceivedRxBuf(uint8_t rxUsage, uint8_t rxPeakUsage);
    void onGpsPropertyUpdatedGnss(const std::vector<GnssSatellite>& sats, std::chrono::duration<double> lastUpdated);
    void onUBXReceivedGnssConfig(uint8_t numTrkCh, const std::vector<GnssConfigStruct>& gnssConfigs);
    void onUBXReceivedTP5(const UbxTimePulseStruct& tp);
    void onGpsMonHWUpdated(const GnssMonHwStruct& hw);
    void onGpsMonHW2Updated(const GnssMonHw2Struct& hw2);
    void receivedTcpMessage(TcpMessage tcpMessage);
    void pollAllUbxMsgRate();
    void sendGpioPinEvent(uint8_t gpio_pin);
    void onGpsPropertyUpdatedGeodeticPos(const GeodeticPos& pos);
    void UBXReceivedVersion(const QString& swString, const QString& hwString, const QString& protString);
    void sampleAdc0Event();
    void sampleAdc0TraceEvent();
    void sampleAdcEvent(uint8_t channel);
    void getTemperature();
    void scanI2cBus();
    void onUBXReceivedTimeTM2(const UbxTimeMarkStruct& tm);
    void onLogParameterPolled();
    void sendMqttStatus(bool connected);

signals:
    void sendTcpMessage(TcpMessage tcpMessage);
    void closeConnection(QString closeAddress);
    void logParameter(const LogParameter& log);
    void aboutToQuit();
    void sendPollUbxMsgRate(uint16_t msgID);
    void sendPollUbxMsg(uint16_t msgID);
    void sendUbxMsg(uint16_t msgID, const std::string& data);
    // difference between msgRate and msg is that CFG-MSG (0x0601) alone is used
    // to set/get the rate for every message so the msgID must be wrapped to the data
    // of a message of type CFG-MSG first
    void UBXSetCfgMsgRate(uint16_t msgID, uint8_t port, uint8_t rate);
    void UBXSetCfgRate(uint8_t measRate, uint8_t navRate);
    void UBXSetCfgPrt(uint8_t gpsPort, uint8_t outProtocolMask);
    void UBXSetDynModel(uint8_t model);
    void resetUbxDevice(uint32_t flags);
    void setGnssConfig(const std::vector<GnssConfigStruct>& gnssConfigs);
    void UBXSetMinMaxSVs(uint8_t minSVs, uint8_t maxSVs);
    void UBXSetMinCNO(uint8_t minCNO);
    void GpioSetInput(unsigned int gpio);
    void GpioSetOutput(unsigned int gpio);
    void GpioSetPullUp(unsigned int gpio);
    void GpioSetPullDown(unsigned int gpio);
    void GpioSetState(unsigned int gpio, bool state);
    void GpioRegisterForCallback(unsigned int gpio, bool edge); // false=falling, true=rising
    void UBXSetCfgTP5(const UbxTimePulseStruct& tp);
    void UBXSetAopCfg(bool enable = true, uint16_t maxOrbErr = 0);
    void UBXSaveCfg(uint8_t devMask = QtSerialUblox::DEV_BBR | QtSerialUblox::DEV_FLASH);
    void setSamplingTriggerSignal(GPIO_PIN signalName);
    void timeMarkIntervalCountUpdate(uint16_t newCounts, double lastInterval);
    void requestMqttConnectionStatus();
    void eventMessage(const QString& messageString);

private slots:
    void onRateBufferReminder();
    void updateOledDisplay();
    void aquireMonitoringParameters();
    void doRateScanIteration(RateScanInfo* info);

private:
    void incomingConnection(qintptr socketDescriptor) override;
    void setPcaChannel(uint8_t channel); // channel 0 to 3
        // 0: coincidence ; 1: xor ; 2: discr 1 ; 3: discr 2
    void setEventTriggerSelection(GPIO_PIN signal);
    void sendPcaChannel();
    void sendEventTriggerSelection();
    void setDacThresh(uint8_t channel, float threshold); // channel 0 or 1 ; threshold in volts
    void sendDacThresh(uint8_t channel);
    void sendDacReadbackValue(uint8_t channel, float voltage);
    void setBiasVoltage(float voltage);
    void saveDacValuesToEeprom();
    void sendBiasVoltage();
    void sendBiasStatus();
    void sendGainSwitchStatus();
    void setBiasStatus(bool status);
    void sendPreampStatus(uint8_t channel);
    void sendPolarityStatus();
    void setUbxMsgRates(QMap<uint16_t, int>& ubxMsgRates);
    void sendUbxMsgRates();
    void sendGpioRates(int number = 0, quint8 whichRate = 0);
    void sendI2cStats();
    void sendSpiStats();
    void sendCalib();
    void sendHistogram(const Histogram& hist);
    void sendLogInfo();
    bool readEeprom();
    void receivedCalibItems(const std::vector<CalibStruct>& newCalibs);
    void setupHistos();
    void rescaleHisto(Histogram& hist, double center, double width);
    void rescaleHisto(Histogram& hist, double center);
    void checkRescaleHisto(Histogram& hist, double newValue);
    void clearHisto(const QString& histoName);
    void setAdcSamplingMode(quint8 mode);
    void startRateScan(uint8_t channel);
    void printTimestamp();
    void delay(int millisecondsWait);

    void rateCounterIntervalActualisation();
    qreal getRateFromCounts(quint8 which_rate);
    void clearRates();

    MCP4728* dac = nullptr;
    ADS1115* adc = nullptr;
    PCA9536* pca = nullptr;
    LM75* lm75 = nullptr;
    EEPROM24AA02* eep = nullptr;
    UbloxI2c* ubloxI2c = nullptr;
    Adafruit_SSD1306* oled = nullptr;
    float biasVoltage = 0.;
    bool biasON = false;
    GPIO_PIN eventTrigger;
    bool gainSwitch = false;
    bool preampStatus[2];
    uint8_t pcaPortMask = 0;
    QVector<float> dacThresh; // do not give values here because of push_back in constructor of deamon
    QPointer<PigpiodHandler> pigHandler;
    QPointer<TDC7200> tdc7200;
    bool spiDevicePresent = false;
    QPointer<TcpConnection> tcpConnection;
    QMap<uint16_t, int> msgRateCfgs;
    int waitingForAppliedMsgRate = 0;
    QPointer<QtSerialUblox> qtGps;
    QPointer<QTcpServer> tcpServer;
    QString peerAddress;
    QHostAddress daemonAddress = QHostAddress::Null;
    quint16 peerPort, daemonPort;
    QString gpsdevname;
    int verbose, baudrate;
    int gpsTimeout = 5000;
    bool dumpRaw, configGnss, showout, showin;
    bool mqttConnectionStatus = false;
    bool polarity1 = true; // input polarity switch: true=pos, false=neg
    bool polarity2 = true;

    // file handling
    QPointer<FileHandler> fileHandler;

    // mqtt
    QPointer<MuonPi::MqttHandler> mqttHandler;

    // signal handling
    static int sighupFd[2];
    static int sigtermFd[2];
    static int sigintFd[2];

    QPointer<QSocketNotifier> snHup;
    QPointer<QSocketNotifier> snTerm;
    QPointer<QSocketNotifier> snInt;

    // calibration
    ShowerDetectorCalib* calib = nullptr;

    // histograms
    QMap<QString, Histogram> histoMap;

    // others
    QVector<QPointF> xorRatePoints, andRatePoints;
    timespec startOfProgram, lastRateInterval;
    quint32 rateBufferTime = 60; // in s: 60 seconds
    quint32 rateBufferInterval = 2000; // in ms: 2 seconds
    quint32 rateMaxShowInterval = 60 * 60 * 1000; // in ms: 1 hour
    QTimer rateBufferReminder;
    QTimer oledUpdateTimer;
    QList<quint64> andCounts, xorCounts;
    UbxDopStruct currentDOP;
    Property nrSats, nrVisibleSats, fixStatus;
    QVector<QTcpSocket*> peerList;
    QList<float> adcSamplesBuffer;
    uint8_t adcSamplingMode = ADC_MODE_PEAK;
    qint16 currentAdcSampleIndex = -1;
    QTimer samplingTimer;
    QTimer parameterMonitorTimer;
    QTimer rateScanTimer;
    QMap<QString, Property> propertyMap;
    LogEngine logEngine;

    // threads
    QPointer<QThread> mqttHandlerThread;
    QPointer<QThread> fileHandlerThread;
    QPointer<QThread> pigThread;
    QPointer<QThread> gpsThread;
    QPointer<QThread> tcpThread;

    configuration config;
};

#endif // DAEMON_H
