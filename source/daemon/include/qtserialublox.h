#ifndef QTSERIALUBLOX_H
#define QTSERIALUBLOX_H

#include <ublox_structs.h>
#include <QLocale>
#include <QObject>
#include <QPointer>
#include <QSerialPort>
#include <QTimer>
#include <queue>
#include <string>

struct GeodeticPos;
struct GnssMonHwStruct;
struct GnssMonHw2Struct;
struct UbxTimeMarkStruct;

class QtSerialUblox : public QObject {
    Q_OBJECT

public:
    enum { RESET_HOT = 0x00000000,
        RESET_WARM = 0x00010000,
        RESET_COLD = 0xFFFF0000,
        RESET_HW = 0x000000,
        RESET_SW = 0x00000001,
        RESET_SW_GNSS = 0x00000002,
        RESET_HW_AFTER_SHUTDOWN = 0x00000004,
        GNSS_STOP = 0x00000008,
        GNSS_START = 0x00000009 };

    enum { DEV_BBR = 0x01,
        DEV_FLASH = 0x02,
        DEV_EEPROM = 0x04,
        DEV_SPI_FLASH = 0x10 };

    explicit QtSerialUblox(const QString serialPortName, int newTimeout, int baudRate,
        bool newDumpRaw, int newVerbose, bool newShowout, bool newShowin, QObject* parent = 0);

signals:
    // all messages coming from QtSerialUblox class that should be displayed on console
    // get sent to Client thread with signal/slot mechanics
    void toConsole(QString data);
    void UBXReceivedAckNak(uint16_t ackedMsgID, uint16_t ackedCfgMsgID);
    // ackedMsgID contains the return value of the ack msg (in case of CFG_MSG that is CFG_MSG)
    void UBXreceivedMsgRateCfg(uint16_t msgID, uint8_t rate);
    void UBXCfgError(QString data);
    void gpsRestart();
    void gpsConnectionError();
    // information about updated properties
    void gpsPropertyUpdatedUint8(uint8_t data,
        std::chrono::duration<double> updateAge,
        char propertyName);
    void gpsPropertyUpdatedUint32(uint32_t data,
        std::chrono::duration<double> updateAge,
        char propertyName);
    void gpsPropertyUpdatedInt32(int32_t data,
        std::chrono::duration<double> updateAge,
        char propertyName);
    void gpsPropertyUpdatedGnss(std::vector<GnssSatellite>,
        std::chrono::duration<double> updateAge);
    void gpsPropertyUpdatedGeodeticPos(GeodeticPos pos);
    void timTM2(QString timTM2String);
    void UBXReceivedTimeTM2(const UbxTimeMarkStruct& tm);
    void gpsVersion(const QString& swVersion, const QString& hwVersion, const QString& protVersion);
    void gpsMonHW(const GnssMonHwStruct& hw);
    void gpsMonHW2(const GnssMonHw2Struct& hw2);
    void UBXReceivedGnssConfig(uint8_t numTrkCh, const std::vector<GnssConfigStruct>& gnssConfigs);
    void UBXReceivedTP5(const UbxTimePulseStruct& tp);
    void UBXReceivedDops(const UbxDopStruct& dops);
    void UBXReceivedTxBuf(uint8_t txUsage, uint8_t txPeakUsage);
    void UBXReceivedRxBuf(uint8_t rxUsage, uint8_t rxPeakUsage);

public slots:
    // all functions that can be called from other classes through signal/slot mechanics
    void makeConnection();
    void onReadyRead();
    void onRequestGpsProperties();
    void pollMsgRate(uint16_t msgID);
    void pollMsg(uint16_t msgID);
    void enqueueMsg(uint16_t msgID, const std::string& payload);
    // for polling the port configuration for specific port set rate to port ID
    void UBXSetCfgMsgRate(uint16_t msgID, uint8_t port, uint8_t rate);
    void UBXSetCfgRate(uint16_t measRate, uint16_t navRate);
    void UBXSetCfgPrt(uint8_t port, uint8_t outProtocolMask);
    void UBXReset(uint32_t resetFlags = RESET_WARM | RESET_SW);
    void onSetGnssConfig(const std::vector<GnssConfigStruct>& gnssConfigs);
    void UBXSetMinMaxSVs(uint8_t minSVs, uint8_t maxSVs);
    void UBXSetMinCNO(uint8_t minCNO);
    void UBXSetCfgTP5(const UbxTimePulseStruct& tp);
    void UBXSetAopCfg(bool enable = true, uint16_t maxOrbErr = 0);
    void UBXSaveCfg(uint8_t devMask = DEV_BBR | DEV_FLASH);

    void closeAll();

    void ackTimeout();
    // outPortMask is something like 1 for only UBX protocol or 0b11 for UBX and NMEA

    void setDynamicModel(uint8_t model);
    static const std::string& getProtVersionString() { return fProtVersionString; }
    static double getProtVersion();

private:
    // all functions for sending and receiving raw data used by other functions in "public slots" section
    // and scanning raw data up to the point where "UbxMessage" object is generated
    bool scanUnknownMessage(std::string& buffer, UbxMessage& message);
    void calcChkSum(const std::string& buf, unsigned char* chkA, unsigned char* chkB);
    bool sendUBX(uint16_t msgID, const std::string& payload, uint16_t nBytes);
    bool sendUBX(uint16_t msgID, unsigned char* payload, uint16_t nBytes);
    bool sendUBX(UbxMessage& msg);
    void sendQueuedMsg(bool afterTimeout = false);
    void delay(int millisecondsWait);

    // all functions only used for processing and showing "UbxMessage"
    void processMessage(const UbxMessage& msg);
    bool UBXNavClock(uint32_t& itow, int32_t& bias, int32_t& drift,
        uint32_t& tAccuracy, uint32_t& fAccuracy);
    bool UBXTimTP(uint32_t& itow, int32_t& quantErr, uint16_t& weekNr);
    bool UBXTimTP(const std::string& msg);
    bool UBXTimTM2(const std::string& msg);
    std::vector<GnssSatellite> UBXNavSat(const std::string& msg, bool allSats);
    std::vector<GnssSatellite> UBXNavSVinfo(const std::string& msg, bool allSats);
    GeodeticPos UBXNavPosLLH(const std::string& msg);
    void UBXCfgGNSS(const std::string& msg);
    void UBXCfgNav5(const std::string& msg);
    std::vector<std::string> UBXMonVer();
    void UBXNavClock(const std::string& msg);
    void UBXNavTimeGPS(const std::string& msg);
    void UBXNavTimeUTC(const std::string& msg);
    void UBXNavStatus(const std::string& msg);
    void UBXMonHW(const std::string& msg);
    void UBXMonHW2(const std::string& msg);
    void UBXMonTx(const std::string& msg);
    void UBXMonRx(const std::string& msg);
    void UBXMonVer(const std::string& msg);
    void UBXCfgNavX5(const std::string& msg);
    void UBXCfgAnt(const std::string& msg);
    void UBXCfgTP5(const std::string& msg);
    void UBXNavDOP(const std::string& msg);

    static std::string toStdString(unsigned char* data, int dataSize);

    // all global variables used in QtSerialUblox class until UbxMessage was created
    QPointer<QSerialPort> serialPort;
    QString _portName;
    std::string m_buffer = "";
    int _baudRate = 0;
    int verbose = 0;
    int timeout = 5000;
    bool dumpRaw = false; // if true show all messages coming from the gps board that can
        // be interpreted as QString by QString(message) (basically all NMEA)
    bool discardAllNMEA = true; // if true discard all NMEA messages and do not parse them
    bool showout = false; // if true show the ubx messages sent to the gps board as hex
    bool showin = false;
    std::queue<UbxMessage> outMsgBuffer;
    UbxMessage* msgWaitingForAck = nullptr;
    QPointer<QTimer> ackTimer;
    int sendRetryCounter = 0;

    // all global variables used for keeping track of satellites and statistics (gpsProperty)
    gpsProperty<int> leapSeconds;
    gpsProperty<double> noise;
    gpsProperty<double> agc;
    gpsProperty<uint8_t> fix;
    gpsProperty<uint8_t> nrSats;
    gpsProperty<uint8_t> TPQuantErr;
    gpsProperty<uint8_t> txBufUsage;
    gpsProperty<uint8_t> txBufPeakUsage;
    gpsProperty<uint32_t> timeAccuracy;
    gpsProperty<uint32_t> freqAccuracy;
    gpsProperty<uint32_t> eventCounter;
    gpsProperty<int32_t> clkBias;
    gpsProperty<int32_t> clkDrift;
    gpsProperty<std::vector<GnssSatellite>> m_satList;
    gpsProperty<GeodeticPos> geodeticPos;
    const int MSGTIMEOUT = 1500;
    std::queue<gpsTimestamp> fTimestamps;
    static std::string fProtVersionString;
};

#endif // QTSERIALUBLOX_H
