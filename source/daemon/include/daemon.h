#ifndef DAEMON_H
#define DAEMON_H

#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QSocketNotifier>
#include <QPointF>
#include <QTimer>
#include <QVariant>
#include <time.h>

#include <tcpconnection.h>
#include <custom_io_operators.h>
#include <qtserialublox.h>
#include <pigpiodhandler.h>
#include <tdc7200.h>
#include <filehandler.h>
#include <mqtthandler.h>
#include <i2c/i2cdevices.h>
#include <calibration.h>
#include <logparameter.h>
#include <histogram.h>
#include <logengine.h>
#include <ratebuffer.h>

// for sig handling:
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>

struct CalibStruct;
struct UbxTimeMarkStruct;

//enum GPIO_SIGNAL;

class Property {
public:
    Property()=default;

    template <typename T>
    Property(const T& val) : name(""), unit ("") {
        typeId = qMetaTypeId<T>();
        if (typeId==QMetaType::UnknownType) {
            typeId = qRegisterMetaType<T>();
        }
        value=QVariant(val);
        updated=true;
    }

    template <typename T>
    Property(const QString& a_name, const T& val, const QString& a_unit = "")
        : name(a_name), unit(a_unit)
    {
        typeId = qMetaTypeId<T>();
        if (typeId==QMetaType::UnknownType) {
            typeId = qRegisterMetaType<T>();
        }
        value=QVariant(val);
        updated=true;
    }

    Property(const Property& prop) = default;
    Property& operator=(const Property& prop) {
        name=prop.name;
        unit=prop.unit;
        value=prop.value;
        typeId=prop.typeId;
        updated=prop.updated;
        return *this;
    }

    Property& operator=(const QVariant& val) {
        value = val;
        //lastUpdate = std::chrono::system_clock::now();
        updated = true;
        return *this;
    }

    const QVariant& operator()() {
        updated = false;
        return value;
    }

    bool isUpdated() const { return updated; }
//    QMetaType::Type type() const { return static_cast<QMetaType::Type>(value.type()); }
    int type() const { return typeId; }

    QString name="";
    QString unit="";

private:
    QVariant value;
    bool updated=false;
    int typeId=0;
};

struct RateScanInfo {
    uint8_t origPcaMask=0;
    GPIO_SIGNAL origEventTrigger=GPIO_SIGNAL::UNDEFINED_SIGNAL;
    uint16_t lastEvtCounter=0;
    uint8_t thrChannel=0;
    float origThr=3.3;
    float thrIncrement=0.1;
    float minThr=0.05;
    float maxThr=3.3;
    float currentThr=0.;
    uint16_t nrLoops=0;
    uint16_t currentLoop=0;
};

struct RateScan {
//	void addScanPoint(double scanpar, double a_rate) { scanMap[scanpar].append(a_rate); }
    uint8_t origPcaMask=0;
    GPIO_SIGNAL origEventTrigger=GPIO_SIGNAL::UNDEFINED_SIGNAL;
    float origScanPar=3.3;
    double minScanPar=0.;
    double maxScanPar=1.;
    double currentScanPar=0.;
    double scanParIncrement=0.;
    uint32_t currentCounts=0;
    double currentTimeInterval=0.;
    double maxTimeInterval=1.;
    uint16_t nrLoops=0;
    uint16_t currentLoop=0;
    QMap<double, double> scanMap;
};

class Daemon : public QTcpServer
{
    Q_OBJECT

public:
    Daemon(QString username, QString password, QString new_gpsdevname, int new_verbose, quint8 new_pcaPortMask,
        float *new_dacThresh, float new_biasVoltage, bool bias_ON, bool new_dumpRaw, int new_baudrate,
        bool new_configGnss, unsigned int new_eventTrigger, QString new_PeerAddress, quint16 new_PpeerPort,
        QString new_serverAddress, quint16 new_serverPort, bool new_showout, bool new_showin, bool preamp1, bool preamp2, bool gain, QString station_ID,
        bool new_polarity1, bool new_polarity2, QObject *parent = 0);
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
    void onGpioPinEvent(unsigned int gpio, EventTime event_time);
    void sendGpioPinEvent(unsigned int gpio, EventTime event_time);
    void onGpsPropertyUpdatedGeodeticPos(const GeodeticPos& pos);
    void UBXReceivedVersion(const QString& swString, const QString& hwString, const QString& protString);
    void sampleAdc0Event();
    void sampleAdc0TraceEvent();
    void sampleAdcEvent(uint8_t channel);
    void getTemperature();
    void scanI2cBus();
    //void onUBXReceivedTimeTM2(timespec rising, timespec falling, uint32_t accEst, bool valid, uint8_t timeBase, bool utcAvailable);
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
    void GpioSetOutput(unsigned int gpio, bool initState = false);
    void GpioSetPullUp(unsigned int gpio);
    void GpioSetPullDown(unsigned int gpio);
    void GpioSetState(unsigned int gpio, bool state);
    void GpioRegisterForCallback(unsigned int gpio, PigpiodHandler::EventEdge edge); // false=falling, true=rising
    void UBXSetCfgTP5(const UbxTimePulseStruct& tp);
    void UBXSetAopCfg(bool enable=true, uint16_t maxOrbErr=0);
    void UBXSaveCfg(uint8_t devMask=QtSerialUblox::DEV_BBR | QtSerialUblox::DEV_FLASH);
    void setSamplingTriggerSignal(unsigned int gpio);
    void timeMarkIntervalCountUpdate(uint16_t newCounts, double lastInterval);
    void requestMqttConnectionStatus();
	void eventMessage(const QString& messageString);
	void eventInterval(quint64 nsecs);

private slots:
    void onRateBufferReminder();
    void updateOledDisplay();
    void aquireMonitoringParameters();
    void doRateScanIteration(RateScanInfo* info);


private:
    void incomingConnection(qintptr socketDescriptor) override;
    void setPcaChannel(uint8_t channel); // channel 0 to 3
                                             // 0: coincidence ; 1: xor ; 2: discr 1 ; 3: discr 2
    void setEventTriggerSelection(GPIO_SIGNAL signal);
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
    void setUbxMsgRates(QMap<uint16_t,int>& ubxMsgRates);
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
    GPIO_SIGNAL eventTrigger { UNDEFINED_SIGNAL };
    bool gainSwitch = false;
    bool preampStatus[2];
    uint8_t pcaPortMask = 0;
    QVector <float> dacThresh; // do not give values here because of push_back in constructor of deamon
    QPointer<PigpiodHandler> pigHandler;
    QPointer<TDC7200> tdc7200;
    bool spiDevicePresent = false;
    QPointer<TcpConnection> tcpConnection;
    QMap <uint16_t, int> msgRateCfgs;
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
    bool mqttConnectionStatus=false;
    bool polarity1 = true;	// input polarity switch: true=pos, false=neg
    bool polarity2 = true;

    // file handling
    QPointer<FileHandler> fileHandler;

    // mqtt
    QPointer<MuonPi::MqttHandler> mqttHandler;

	// rate buffer
	RateBuffer rateBuffer;
	
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
/*
     Histogram geoHeightHisto, geoLonHisto, geoLatHisto,
     weightedGeoHeightHisto,
     pulseHeightHisto, adcSampleTimeHisto, tdc7200Histo,
     ubxTimeLengthHisto, eventIntervalHisto, eventIntervalShortHisto,
     ubxTimeIntervalHisto, tpTimeDiffHisto;
*/
    QMap<QString, Histogram> histoMap;

    // others
    QVector<QPointF> xorRatePoints, andRatePoints;
    timespec startOfProgram, lastRateInterval;
    quint32 rateBufferTime = 60; // in s: 60 seconds
    quint32 rateBufferInterval = 2000; // in ms: 2 seconds
    quint32 rateMaxShowInterval = 60*60*1000; // in ms: 1 hour
    QTimer rateBufferReminder;
    QTimer oledUpdateTimer;
    QList<quint64> andCounts,xorCounts;
    UbxDopStruct currentDOP;
    //timespec lastTimestamp = { 0, 0 };
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
};

#endif // DAEMON_H
