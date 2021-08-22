#ifndef MUONDETECTOR_STRUCTS_H
#define MUONDETECTOR_STRUCTS_H

#include "histogram.h"
#include "muondetector_shared_global.h"

#include <QDataStream>
#include <QList>
#include <QMap>
#include <QString>
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/types.h>

enum ADC_SAMPLING_MODE { ADC_MODE_DISABLED = 0,
    ADC_MODE_PEAK = 1,
    ADC_MODE_TRACE = 2 };
enum TIMING_MUX_INPUTS { MUX_AND = 0,
    MUX_XOR = 1,
    MUX_DISC1 = 2,
    MUX_DISC2 = 3 };

struct CalibStruct {
public:
    enum { CALIBFLAGS_NO_CALIB = 0x00,
        CALIBFLAGS_COMPONENTS = 0x01,
        CALIBFLAGS_VOLTAGE_COEFFS = 0x02,
        CALIBFLAGS_CURRENT_COEFFS = 0x04 };

    enum { FEATUREFLAGS_NONE = 0x00,
        FEATUREFLAGS_GNSS = 0x01,
        FEATUREFLAGS_ENERGY = 0x02,
        FEATUREFLAGS_DETBIAS = 0x04,
        FEATUREFLAGS_PREAMP_BIAS = 0x08 };

    CalibStruct() = default;
    CalibStruct(const std::string& a_name, const std::string& a_type, uint8_t a_address, const std::string& a_value)
        : name(a_name)
        , type(a_type)
        , address(a_address)
        , value(a_value)
    {
    }
    ~CalibStruct() = default;
    CalibStruct(const CalibStruct& s)
        : name(s.name)
        , type(s.type)
        , address(s.address)
        , value(s.value)
    {
    }
    std::string name = "";
    std::string type = "";
    uint16_t address = 0;
    std::string value = "";
};

struct GeodeticPos {
    uint32_t iTOW;
    int32_t lon; // longitude 1e-7 scaling (increase by 1 means 100 nano degrees)
    int32_t lat; // latitude 1e-7 scaling (increase by 1 means 100 nano degrees)
    int32_t height; // height above ellipsoid
    int32_t hMSL; // height above main sea level
    uint32_t hAcc; // horizontal accuracy estimate
    uint32_t vAcc; // vertical accuracy estimate
};

struct GnssConfigStruct {
    uint8_t gnssId;
    uint8_t resTrkCh;
    uint8_t maxTrkCh;
    uint32_t flags;
};

class GnssSatellite {
public:
    GnssSatellite() { }
    GnssSatellite(int gnssId, int satId, int cnr, int elev, int azim, float prRes, uint32_t flags)
        : fGnssId(gnssId)
        , fSatId(satId)
        , fCnr(cnr)
        , fElev(elev)
        , fAzim(azim)
        , fPrRes(prRes)
    {
        fQuality = (int)(flags & 0x07);
        if (flags & 0x08)
            fUsed = true;
        else
            fUsed = false;
        fHealth = (int)(flags >> 4 & 0x03);
        fOrbitSource = (flags >> 8 & 0x07);
        fSmoothed = (flags & 0x80);
        fDiffCorr = (flags & 0x40);
    }

    GnssSatellite(int gnssId, int satId, int cnr, int elev, int azim, float prRes,
        int quality, int health, int orbitSource, bool used, bool diffCorr, bool smoothed)
        : fGnssId(gnssId)
        , fSatId(satId)
        , fCnr(cnr)
        , fElev(elev)
        , fAzim(azim)
        , fPrRes(prRes)
        , fQuality(quality)
        , fHealth(health)
        , fOrbitSource(orbitSource)
        , fUsed(used)
        , fDiffCorr(diffCorr)
        , fSmoothed(smoothed)
    {
    }

    ~GnssSatellite() { }

    static void PrintHeader(bool wIndex);
    void Print(bool wHeader) const;
    void Print(int index, bool wHeader) const;

    static bool sortByCnr(const GnssSatellite& sat1, const GnssSatellite& sat2)
    {
        return sat1.getCnr() > sat2.getCnr();
    }

    inline int getCnr() const { return fCnr; }

    friend QDataStream& operator<<(QDataStream& out, const GnssSatellite& sat);
    friend QDataStream& operator>>(QDataStream& in, GnssSatellite& sat);

public:
    int fGnssId = 0, fSatId = 0, fCnr = 0, fElev = 0, fAzim = 0;
    float fPrRes = 0.;
    int fQuality = 0, fHealth = 0;
    int fOrbitSource = 0;
    bool fUsed = false, fDiffCorr = false, fSmoothed = false;
};

struct UbxTimePulseStruct {
    enum { ACTIVE = 0x01,
        LOCK_GPS = 0x02,
        LOCK_OTHER = 0x04,
        IS_FREQ = 0x08,
        IS_LENGTH = 0x10,
        ALIGN_TO_TOW = 0x20,
        POLARITY = 0x40,
        GRID_UTC_GPS = 0x780 };
    uint8_t tpIndex = 0;
    uint8_t version = 0;
    int16_t antCableDelay = 0;
    int16_t rfGroupDelay = 0;
    uint32_t freqPeriod = 0;
    uint32_t freqPeriodLock = 0;
    uint32_t pulseLenRatio = 0;
    uint32_t pulseLenRatioLock = 0;
    int32_t userConfigDelay = 0;
    uint32_t flags = 0;
};

struct UbxTimeMarkStruct {
    enum { TIMEBASE_LOCAL = 0x00,
        TIMEBASE_GNSS = 0x01,
        TIMEBASE_UTC = 0x02,
        TIMEBASE_OTHER = 0x03 };
    struct timespec rising = { 0, 0 };
    struct timespec falling = { 0, 0 };
    bool risingValid = false;
    bool fallingValid = false;
    uint32_t accuracy_ns = 0;
    bool valid = false;
    uint8_t timeBase = 0;
    bool utcAvailable = false;
    uint8_t flags = 0;
    uint16_t evtCounter = 0;
};

struct GnssMonHwStruct {
    GnssMonHwStruct() = default;
    GnssMonHwStruct(quint16 a_noise, quint16 a_agc, quint8 a_antStatus, quint8 a_antPower, quint8 a_jamInd, quint8 a_flags)
        : noise(a_noise)
        , agc(a_agc)
        , antStatus(a_antStatus)
        , antPower(a_antPower)
        , jamInd(a_jamInd)
        , flags(a_flags)
    {
    }
    quint16 noise = 0, agc = 0;
    quint8 antStatus = 0, antPower = 0, jamInd = 0, flags = 0;
};

struct GnssMonHw2Struct {
    GnssMonHw2Struct() = default;
    GnssMonHw2Struct(qint8 a_ofsI, qint8 a_ofsQ, quint8 a_magI, quint8 a_magQ, quint8 a_cfgSrc)
        : ofsI(a_ofsI)
        , ofsQ(a_ofsQ)
        , magI(a_magI)
        , magQ(a_magQ)
        , cfgSrc(a_cfgSrc)
    {
    }
    qint8 ofsI = 0, ofsQ = 0;
    quint8 magI = 0, magQ = 0;
    quint8 cfgSrc = 0;
};

enum I2C_DEVICE_MODE { I2C_MODE_NONE = 0,
    I2C_MODE_NORMAL = 0x01,
    I2C_MODE_FORCE = 0x02,
    I2C_MODE_UNREACHABLE = 0x04,
    I2C_MODE_FAILED = 0x08,
    I2C_MODE_LOCKED = 0x10 };

struct I2cDeviceEntry {
    quint8 address;
    QString name;
    quint8 status;
    quint32 nrBytesWritten;
    quint32 nrBytesRead;
    quint32 nrIoErrors;
    quint32 lastTransactionTime; // in us
};

struct LogInfoStruct {
    QString logFileName;
    QString dataFileName;
    quint8 status;
    quint32 logFileSize;
    quint32 dataFileSize;
    qint32 logAge;
};

struct OledItem {
    QString name;
    QString displayString;
};

static const QList<QString> GNSS_ID_STRING = { " GPS", "SBAS", " GAL", "BEID", "IMES", "QZSS", "GLNS", " N/A" };
static const QList<QString> FIX_TYPE_STRINGS = { "No Fix", "Dead Reck.", "2D-Fix", "3D-Fix", "GPS+Dead Reck.", "Time Fix" };
static const QList<QString> GNSS_ORBIT_SRC_STRING = { "N/A", "Ephem", "Alm", "AOP", "AOP+", "Alt", "Alt", "Alt" };
static const QList<QString> GNSS_ANT_STATUS_STRINGS = { "init", "unknown", "ok", "short", "open", "unknown", "unknown" };
static const QList<QString> GNSS_HEALTH_STRINGS = { "N/A", "good", "bad", "bad+" };
static const QMap<quint8, QString> I2C_MODE_STRINGMAP = { { 0x00, "None" },
    { 0x01, "Normal" },
    { 0x02, "System" },
    { 0x04, "Unreachable" },
    { 0x08, "Failed" },
    { 0x10, "Locked" } };

inline void GnssSatellite::PrintHeader(bool wIndex)
{
    if (wIndex) {
        std::cout << "   ----------------------------------------------------------------------------------" << std::endl;
        std::cout << "   Nr   Sys    ID   S/N(dB)  El(deg)  Az(deg)  Res(m) Qlty Use Hlth Src Smth DiffCorr" << std::endl;
        std::cout << "   ----------------------------------------------------------------------------------" << std::endl;
    } else {
        std::cout << "   -----------------------------------------------------------------" << std::endl;
        std::cout << "    Sys    ID   S/N(dB)  El(deg)  Az(deg)  Res(m) Qlty Use Hlth Src Smth DiffCorr" << std::endl;
        std::cout << "   -----------------------------------------------------------------" << std::endl;
    }
}

inline void GnssSatellite::Print(bool wHeader) const
{
    if (wHeader) {
        std::cout << "   ------------------------------------------------------------------------------" << std::endl;
        std::cout << "    Sys    ID   S/N(dB)  El(deg)  Az(deg)  Res(m) Qlty Use Hlth Src Smth DiffCorr" << std::endl;
        std::cout << "   ------------------------------------------------------------------------------" << std::endl;
    }
    std::cout << "   " << std::dec << "  " << GNSS_ID_STRING[(int)fGnssId].toStdString() << "   " << std::setw(3) << (int)fSatId << "    ";
    std::cout << std::setw(3) << (int)fCnr << "      " << std::setw(3) << (int)fElev << "       " << std::setw(3) << (int)fAzim;
    std::cout << "   " << std::setw(6) << fPrRes << "    " << fQuality << "   " << std::string((fUsed) ? "Y" : "N");
    std::cout << "    " << fHealth << "   " << fOrbitSource << "   " << (int)fSmoothed << "    " << (int)fDiffCorr;
    std::cout << std::endl;
}

inline void GnssSatellite::Print(int index, bool wHeader) const
{
    if (wHeader) {
        std::cout << "   ----------------------------------------------------------------------------------" << std::endl;
        std::cout << "   Nr   Sys    ID   S/N(dB)  El(deg)  Az(deg)  Res(m) Qlty Use Hlth Src Smth DiffCorr" << std::endl;
        std::cout << "   ----------------------------------------------------------------------------------" << std::endl;
    }
    std::cout << "   " << std::dec << std::setw(2) << index + 1 << "  " << GNSS_ID_STRING[(int)fGnssId].toStdString() << "   " << std::setw(3) << (int)fSatId << "    ";
    std::cout << std::setw(3) << (int)fCnr << "      " << std::setw(3) << (int)fElev << "       " << std::setw(3) << (int)fAzim;
    std::cout << "   " << std::setw(6) << fPrRes << "    " << fQuality << "   " << std::string((fUsed) ? "Y" : "N");
    std::cout << "    " << fHealth << "   " << fOrbitSource << "   " << (int)fSmoothed << "    " << (int)fDiffCorr;
    ;
    std::cout << std::endl;
}

inline QDataStream& operator<<(QDataStream& out, const CalibStruct& calib)
{
    out << QString::fromStdString(calib.name) << QString::fromStdString(calib.type)
        << (quint16)calib.address << QString::fromStdString(calib.value);
    return out;
}

inline QDataStream& operator>>(QDataStream& in, CalibStruct& calib)
{
    QString s1, s2, s3;
    quint16 u;
    in >> s1 >> s2;
    in >> u;
    in >> s3;
    calib.name = s1.toStdString();
    calib.type = s2.toStdString();
    calib.address = (uint16_t)u;
    calib.value = s3.toStdString();
    return in;
}

inline QDataStream& operator>>(QDataStream& in, GnssSatellite& sat)
{
    in >> sat.fGnssId >> sat.fSatId >> sat.fCnr >> sat.fElev >> sat.fAzim
        >> sat.fPrRes >> sat.fQuality >> sat.fHealth >> sat.fOrbitSource
        >> sat.fUsed >> sat.fDiffCorr >> sat.fSmoothed;
    return in;
}

inline QDataStream& operator<<(QDataStream& out, const GnssSatellite& sat)
{
    out << sat.fGnssId << sat.fSatId << sat.fCnr << sat.fElev << sat.fAzim
        << sat.fPrRes << sat.fQuality << sat.fHealth << sat.fOrbitSource
        << sat.fUsed << sat.fDiffCorr << sat.fSmoothed;
    return out;
}

inline QDataStream& operator>>(QDataStream& in, UbxTimePulseStruct& tp)
{
    in >> tp.tpIndex >> tp.version >> tp.antCableDelay >> tp.rfGroupDelay
        >> tp.freqPeriod >> tp.freqPeriodLock >> tp.pulseLenRatio >> tp.pulseLenRatioLock
        >> tp.userConfigDelay >> tp.flags;
    return in;
}

inline QDataStream& operator<<(QDataStream& out, const UbxTimePulseStruct& tp)
{
    out << tp.tpIndex << tp.version << tp.antCableDelay << tp.rfGroupDelay
        << tp.freqPeriod << tp.freqPeriodLock << tp.pulseLenRatio << tp.pulseLenRatioLock
        << tp.userConfigDelay << tp.flags;
    return out;
}

inline QDataStream& operator>>(QDataStream& in, Histogram& h)
{
    h.clear();
    QString name, unit;
    in >> name >> h.fMin >> h.fMax >> h.fUnderflow >> h.fOverflow >> h.fNrBins;
    h.setName(name.toStdString());
    for (int i = 0; i < h.fNrBins; i++) {
        in >> h.fHistogramMap[i];
    }
    in >> unit;
    h.setUnit(unit.toStdString());
    return in;
}

inline QDataStream& operator<<(QDataStream& out, const Histogram& h)
{
    out << QString::fromStdString(h.fName) << h.fMin << h.fMax << h.fUnderflow << h.fOverflow << h.fNrBins;
    for (int i = 0; i < h.fNrBins; i++) {
        out << h.getBinContent(i);
    }
    out << QString::fromStdString(h.fUnit);
    return out;
}

inline QDataStream& operator>>(QDataStream& in, GnssMonHwStruct& hw)
{
    in >> hw.noise >> hw.agc >> hw.antStatus >> hw.antPower >> hw.jamInd >> hw.flags;
    return in;
}

inline QDataStream& operator<<(QDataStream& out, const GnssMonHwStruct& hw)
{
    out << hw.noise << hw.agc << hw.antStatus << hw.antPower << hw.jamInd << hw.flags;
    return out;
}

inline QDataStream& operator>>(QDataStream& in, GnssMonHw2Struct& hw2)
{
    in >> hw2.ofsI >> hw2.magI >> hw2.ofsQ >> hw2.magQ >> hw2.cfgSrc;
    return in;
}

inline QDataStream& operator<<(QDataStream& out, const GnssMonHw2Struct& hw2)
{
    out << hw2.ofsI << hw2.magI << hw2.ofsQ << hw2.magQ << hw2.cfgSrc;
    return out;
}

inline QDataStream& operator>>(QDataStream& in, LogInfoStruct& lis)
{
    in >> lis.logFileName >> lis.dataFileName >> lis.status >> lis.logFileSize
        >> lis.dataFileSize >> lis.logAge;
    return in;
}

inline QDataStream& operator<<(QDataStream& out, const LogInfoStruct& lis)
{
    out << lis.logFileName << lis.dataFileName << lis.status << lis.logFileSize
        << lis.dataFileSize << lis.logAge;
    return out;
}

inline QDataStream& operator>>(QDataStream& in, UbxTimeMarkStruct& tm)
{
    qint64 sec, nsec;
    in >> sec >> nsec;
    tm.rising.tv_sec = sec;
    tm.rising.tv_nsec = nsec;
    in >> sec >> nsec;
    tm.falling.tv_sec = sec;
    tm.falling.tv_nsec = nsec;
    in >> tm.risingValid >> tm.fallingValid >> tm.accuracy_ns >> tm.valid
        >> tm.timeBase >> tm.utcAvailable >> tm.flags >> tm.evtCounter;
    return in;
}

inline QDataStream& operator<<(QDataStream& out, const UbxTimeMarkStruct& tm)
{
    out << (qint64)tm.rising.tv_sec << (qint64)tm.rising.tv_nsec << (qint64)tm.falling.tv_sec << (qint64)tm.falling.tv_nsec
        << tm.risingValid << tm.fallingValid << tm.accuracy_ns << tm.valid
        << tm.timeBase << tm.utcAvailable << tm.flags << tm.evtCounter;
    return out;
}

#endif // MUONDETECTOR_STRUCTS_H
