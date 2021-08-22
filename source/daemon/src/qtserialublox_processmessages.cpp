#include "utility/custom_io_operators.h"
#include "utility/unixtime_from_gps.h"
#include "qtserialublox.h"

#include <muondetector_structs.h>
#include <ublox_messages.h>

#include <QThread>
#include <cctype>
#include <iomanip>
#include <sstream>

using namespace std;

const uint8_t usedPort = 1; // this is the uart port. (0 = i2c; 1 = uart; 2 = usb; 3 = isp;)
    // see u-blox8-M8_Receiver... pdf documentation site 170
// all about processing different ubx-messages:
void QtSerialUblox::processMessage(const UbxMessage& msg)
{
    uint8_t classID = (msg.msgID & 0xff00) >> 8;
    uint8_t messageID = msg.msgID & 0xff;
    std::vector<GnssSatellite> sats;
    std::stringstream tempStream;
    uint16_t ackedMsgID;
    switch (classID) {
    case 0x05: // UBX-ACK
        if (msg.data.size() < 2) {
            emit toConsole("received UBX-ACK message but data is corrupted\n");
            break;
        }
        if (!msgWaitingForAck) {
            if (verbose > 1) {
                tempStream << "received ACK message but no message is waiting for Ack (msgID: 0x";
                tempStream << std::setfill('0') << std::setw(2) << std::hex << (int)msg.data[0] << " 0x"
                           << std::setfill('0') << std::setw(2) << std::hex << (int)msg.data[1] << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            break;
        }
        if (verbose > 2) {
            if (messageID == 1)
                tempStream << "received UBX-ACK-ACK message about msgID: 0x";
            else
                tempStream << "received UBX-ACK-NACK message about msgID: 0x";
            tempStream << std::setfill('0') << std::setw(2) << std::hex << (int)msg.data[0] << " 0x"
                       << std::setfill('0') << std::setw(2) << std::hex << (int)msg.data[1] << "\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        ackedMsgID = (uint16_t)(msg.data[0]) << 8 | msg.data[1];
        if (ackedMsgID != msgWaitingForAck->msgID) {
            if (verbose > 1) {
                tempStream << "received unexpected UBX-ACK message about msgID: 0x";
                tempStream << std::setfill('0') << std::setw(2) << std::hex << (int)msg.data[0] << " 0x"
                           << std::setfill('0') << std::setw(2) << std::hex << (int)msg.data[1] << "\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            break;
        }
        switch (messageID) {
        case 0x00:
            emit UBXReceivedAckNak(msgWaitingForAck->msgID,
                (uint16_t)(msgWaitingForAck->data[0]) << 8
                    | msgWaitingForAck->data[1]);
            break;
        default:
            break;
        }
        ackTimer->stop();
        delete msgWaitingForAck;
        msgWaitingForAck = 0;
        if (verbose > 2)
            emit toConsole("processMessage: deleted message after ACK/NACK\n");
        sendQueuedMsg();
        break;
    case 0x01: // UBX-NAV
        switch (messageID) {
        case 0x03:
            if (verbose > 2) {
                tempStream << "received UBX-NAV-STATUS message (0x" << std::hex << std::setfill('0') << std::setw(2)
                           << (int)classID << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXNavStatus(msg.data);
            break;
        case 0x04:
            if (verbose > 2) {
                tempStream << "received UBX-NAV-DOP message (0x" << std::hex << std::setfill('0') << std::setw(2)
                           << (int)classID << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXNavDOP(msg.data);
            break;
        case 0x20:
            if (verbose > 2) {
                tempStream << "received UBX-NAV-TIMEGPS message (0x" << std::hex << std::setfill('0') << std::setw(2)
                           << (int)classID << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXNavTimeGPS(msg.data);
            break;
        case 0x21:
            if (verbose > 2) {
                tempStream << "received UBX-NAV-TIMEUTC message (0x" << std::hex << std::setw(2)
                           << (int)classID << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXNavTimeUTC(msg.data);
            break;
        case 0x22:
            if (verbose > 2) {
                tempStream << "received UBX-NAV-CLOCK message (0x" << std::hex << std::setfill('0') << std::setw(2)
                           << (int)classID << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXNavClock(msg.data);
            break;
        case 0x30:
            sats = UBXNavSVinfo(msg.data, true);
            if (verbose > 2) {
                tempStream << "received UBX-NAV-SVINFO message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                tempStream << "nr sats = " << sats.size() << "\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            emit gpsPropertyUpdatedGnss(sats, m_satList.updateAge());
            m_satList = sats;
            break;
        case 0x35:
            sats = UBXNavSat(msg.data, true);
            if (verbose > 2) {
                tempStream << "received UBX-NAV-SAT message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            emit gpsPropertyUpdatedGnss(sats, m_satList.updateAge());
            m_satList = sats;
            break;
        case 0x02:
            if (verbose > 2) {
                tempStream << "received UBX-NAV-POSLLH message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            geodeticPos = UBXNavPosLLH(msg.data);
            emit gpsPropertyUpdatedGeodeticPos(geodeticPos());
            break;
        default:
            if (verbose > 2) {
                tempStream << "received unhandled UBX-NAV message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
        }
        break;
    case 0x02: // UBX-RXM
        if (verbose > 2) {
            tempStream << "received unhandled UBX-RXM message\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        break;
    case 0x0b: // UBX-AID
        if (verbose > 2) {
            tempStream << "received unhandled UBX-AID message\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        break;
    case 0x06: // UBX-CFG
        switch (messageID) {
        case 0x01: // UBX-CFG-MSG   (message configuration)
            emit UBXreceivedMsgRateCfg(
                (((uint16_t)msg.data[0]) << 8) | ((uint16_t)msg.data[1]),
                (uint8_t)(msg.data[2 + usedPort]));
            // 2: port 0 (i2c); 3: port 1 (uart); 4: port 2 (usb); 5: port 3 (isp)
            break;
        case 0x13: // UBX-CFG-ANT
            if (verbose > 2) {
                tempStream << "received UBX-CFG-ANT message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXCfgAnt(msg.data);
            break;
        case 0x24: // UBX-CFG-NAV5
            if (verbose > 2) {
                tempStream << "received UBX-CFG-NAV5 message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXCfgNav5(msg.data);
            break;
        case 0x23: // UBX-CFG-NAVX5
            if (verbose > 2) {
                tempStream << "received UBX-CFG-NAVX5 message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXCfgNavX5(msg.data);
            break;
        case 0x31: // UBX-CFG-TP5
            if (verbose > 2) {
                tempStream << "received UBX-CFG-TP5 message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXCfgTP5(msg.data);
            break;
        case 0x3e: // UBX-CFG-GNSS
            if (verbose > 2) {
                tempStream << "received UBX-CFG-GNSS message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXCfgGNSS(msg.data);
            break;
        default:
            if (verbose > 2) {
                tempStream << "received unhandled UBX-CFG message:";
                for (std::string::size_type i = 0; i < msg.data.size(); i++) {
                    tempStream << " 0x" << std::setfill('0') << std::setw(2) << std::hex << (int)msg.data[i];
                }
                tempStream << "\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
        }
        break;
    case 0x10: // UBX-ESF
        if (verbose > 2) {
            tempStream << "received unhandled UBX-ESF message\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        break;
    case 0x28: // UBX-HNR
        if (verbose > 2) {
            tempStream << "received unhandled UBX-HNR message\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        break;
    case 0x04: // UBX-INF
        switch (messageID) {
        default:
            if (verbose > 2) {
                tempStream << "received unhandled UBX-INF message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
        }
        break;
    case 0x21: // UBX-LOG
        if (verbose > 2) {
            tempStream << "received unhandled UBX-LOG message\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        break;
    case 0x13: // UBX-MGA
        if (verbose > 2) {
            tempStream << "received unhandled UBX-MGA message\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        break;
    case 0x0a: // UBX-MON
        switch (messageID) {
        case (UBX_MON_RXBUF & 0xff):
            if (verbose > 2) {
                tempStream << "received UBX-MON-RXBUF message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXMonRx(msg.data);
            break;
        case (UBX_MON_TXBUF & 0xff):
            if (verbose > 2) {
                tempStream << "received UBX-MON-TXBUF message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXMonTx(msg.data);
            break;
        case 0x09:
            if (verbose > 2) {
                tempStream << "received UBX-MON-HW message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXMonHW(msg.data);
            break;
        case 0x0b:
            if (verbose > 2) {
                tempStream << "received UBX-MON-HW2 message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXMonHW2(msg.data);
            break;
        case 0x04:
            if (verbose > 2) {
                tempStream << "received UBX-MON-VER message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXMonVer(msg.data);
            break;
        default:
            if (verbose > 2) {
                tempStream << "received unhandled UBX-MON message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
        }
        break;
    case 0x27: // UBX-SEC
        if (verbose > 2) {
            tempStream << "received unhandled UBX-SEC message\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        break;
    case 0x0d: // UBX-TIM
        switch (messageID) {
        case 0x01: // UBX-TIM-TP
            if (verbose > 2) {
                tempStream << "received UBX-TIM-TP message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID << " 0x"
                           << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXTimTP(msg.data);
            break;
        case 0x03: // UBX-TIM-TM2
            if (verbose > 2) {
                tempStream << "received UBX-TIM-TM2 message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID << " 0x"
                           << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
            UBXTimTM2(msg.data);
            break;
        default:
            if (verbose > 2) {
                tempStream << "received unhandled UBX-TIM message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                           << " 0x" << std::hex << (int)messageID << ")\n";
                emit toConsole(QString::fromStdString(tempStream.str()));
            }
        }
        break;
    case 0x09: // UBX-UPD
        if (verbose > 2) {
            tempStream << "received unhandled UBX-UPD message\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        break;
    default:
        if (verbose > 2) {
            tempStream << "received unknown UBX message (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)classID
                       << " 0x" << std::hex << (int)messageID << ")\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        break;
    }
}

bool QtSerialUblox::UBXTimTP(const std::string& msg)
{
    // parse all fields
    // TP time of week, ms
    uint32_t towMS = (int)msg[0];
    towMS += ((int)msg[1]) << 8;
    towMS += ((int)msg[2]) << 16;
    towMS += ((int)msg[3]) << 24;
    // TP time of week, sub ms
    uint32_t towSubMS = (int)msg[4];
    towSubMS += ((int)msg[5]) << 8;
    towSubMS += ((int)msg[6]) << 16;
    towSubMS += ((int)msg[7]) << 24;
    // quantization error
    int32_t qErr = (int)msg[8];
    qErr += ((int)msg[9]) << 8;
    qErr += ((int)msg[10]) << 16;
    qErr += ((int)msg[11]) << 24;
    emit gpsPropertyUpdatedInt32(qErr, TPQuantErr.updateAge(), 'e');
    TPQuantErr = qErr;
    // week number
    uint16_t week = (int)msg[12];
    week += ((int)msg[13]) << 8;
    // flags
    uint8_t flags = msg[14];
    // ref info
    uint8_t refInfo = msg[15];

    double sr = towMS / 1000.;
    sr = sr - towMS / 1000;

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << "*** UBX-TIM-TP message:" << endl;
        tempStream << " tow s            : " << dec << towMS / 1000. << " s" << endl;
        tempStream << " tow sub s        : " << dec << towSubMS << " = " << (long int)(sr * 1e9 + towSubMS + 0.5) << " ns" << endl;
        tempStream << " quantization err : " << dec << qErr << " ps" << endl;
        tempStream << " week nr          : " << dec << week << endl;
        tempStream << " *flags            : ";
        for (int i = 7; i >= 0; i--)
            if (flags & 1 << i)
                tempStream << i;
            else
                tempStream << "-";
        tempStream << endl;
        tempStream << "  time base     : " << string(((flags & 1) ? "UTC" : "GNSS")) << endl;
        tempStream << "  UTC available : " << string((flags & 2) ? "yes" : "no") << endl;
        tempStream << "  (T)RAIM info  : " << (int)((flags & 0x0c) >> 2) << endl;
        tempStream << " *refInfo          : ";
        for (int i = 7; i >= 0; i--)
            if (refInfo & 1 << i)
                tempStream << i;
            else
                tempStream << "-";
        tempStream << endl;
        string gnssRef;
        switch (refInfo & 0x0f) {
        case 0:
            gnssRef = "GPS";
            break;
        case 1:
            gnssRef = "GLONASS";
            break;
        case 2:
            gnssRef = "BeiDou";
            break;
        default:
            gnssRef = "unknown";
        }
        tempStream << "  GNSS reference : " << gnssRef << endl;
        string utcStd;
        switch ((refInfo & 0xf0) >> 4) {
        case 0:
            utcStd = "n/a";
            break;
        case 1:
            utcStd = "CRL";
            break;
        case 2:
            utcStd = "NIST";
            break;
        case 3:
            utcStd = "USNO";
            break;
        case 4:
            utcStd = "BIPM";
            break;
        case 5:
            utcStd = "EU";
            break;
        case 6:
            utcStd = "SU";
            break;
        default:
            utcStd = "unknown";
        }
        tempStream << "  UTC standard  : " << utcStd << "\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    return true;
}

bool QtSerialUblox::UBXTimTM2(const std::string& msg)
{
    // parse all fields
    // channel
    uint8_t ch = msg[0];
    // flags
    uint8_t flags = msg[1];
    // rising edge counter
    uint16_t count = (int)msg[2]; // 16 bit counter
    count += ((int)msg[3]) << 8;
    // week number of last rising edge
    uint16_t wnR = (int)msg[4];
    wnR += ((int)msg[5]) << 8;
    // week number of last falling edge
    uint16_t wnF = (int)msg[6];
    wnF += ((int)msg[7]) << 8;
    // time of week of rising edge, ms
    uint32_t towMsR = (int)msg[8];
    towMsR += ((int)msg[9]) << 8;
    towMsR += ((int)msg[10]) << 16;
    towMsR += ((int)msg[11]) << 24;
    // time of week of rising edge, sub ms
    uint32_t towSubMsR = (int)msg[12];
    towSubMsR += ((int)msg[13]) << 8;
    towSubMsR += ((int)msg[14]) << 16;
    towSubMsR += ((int)msg[15]) << 24;
    // time of week of falling edge, ms
    uint32_t towMsF = (int)msg[16];
    towMsF += ((int)msg[17]) << 8;
    towMsF += ((int)msg[18]) << 16;
    towMsF += ((int)msg[19]) << 24;
    // time of week of falling edge, sub ms
    uint32_t towSubMsF = (int)msg[20];
    towSubMsF += ((int)msg[21]) << 8;
    towSubMsF += ((int)msg[22]) << 16;
    towSubMsF += ((int)msg[23]) << 24;
    // accuracy estimate
    uint32_t accEst = (int)msg[24];
    accEst += ((int)msg[25]) << 8;
    accEst += ((int)msg[26]) << 16;
    accEst += ((int)msg[27]) << 24;

    double sr = towMsR / 1000.;
    sr = sr - towMsR / 1000;
    double sf = towMsF / 1000.;
    sf = sf - towMsF / 1000;

    // meaning of columns:
    // 0d 03 - signature of TIM-TM2 message
    // ch, week nr, second in current week (rising), ns of timestamp in current second (rising),
    // second in current week (falling), ns of timestamp in current second (falling),
    // accuracy (ns), rising edge counter, rising/falling edge (1/0), time valid (GNSS fix)

    std::stringstream tempStream;
    if (verbose > 2) {
        tempStream << "*** UBX-TimTM2 message:" << endl;
        tempStream << " channel         : " << dec << (int)ch << endl;
        tempStream << " rising edge ctr : " << dec << count << endl;
        tempStream << " * last rising edge:" << endl;
        tempStream << "    week nr        : " << dec << wnR << endl;
        tempStream << "    tow s          : " << dec << towMsR / 1000. << " s" << endl;
        tempStream << "    tow sub s     : " << dec << towSubMsR << " = " << (long int)(sr * 1e9 + towSubMsR) << " ns" << endl;
        tempStream << " * last falling edge:" << endl;
        tempStream << "    week nr        : " << dec << wnF << endl;
        tempStream << "    tow s          : " << dec << towMsF / 1000. << " s" << endl;
        tempStream << "    tow sub s      : " << dec << towSubMsF << " = " << (long int)(sf * 1e9 + towSubMsF) << " ns" << endl;
        tempStream << " accuracy est      : " << dec << accEst << " ns" << endl;
        tempStream << " flags             : ";
        for (int i = 7; i >= 0; i--)
            if (flags & 1 << i)
                tempStream << i;
            else
                tempStream << "-";
        tempStream << endl;
        tempStream << "   mode                 : " << string((flags & 1) ? "single" : "running") << endl;
        tempStream << "   run                  : " << string((flags & 2) ? "armed" : "stopped") << endl;
        tempStream << "   new rising edge      : " << string((flags & 0x80) ? "yes" : "no") << endl;
        tempStream << "   new falling edge     : " << string((flags & 0x04) ? "yes" : "no") << endl;
        tempStream << "   UTC available        : " << string((flags & 0x20) ? "yes" : "no") << endl;
        tempStream << "   time valid (GNSS fix): " << string((flags & 0x40) ? "yes" : "no") << endl;
        string timeBase;
        switch ((flags & 0x18) >> 3) {
        case 0:
            timeBase = "receiver time";
            break;
        case 1:
            timeBase = "GNSS";
            break;
        case 2:
            timeBase = "UTC";
            break;
        default:
            timeBase = "unknown";
        }
        tempStream << "   time base            : " << timeBase << "\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }

    tempStream.clear();
    if (flags & 0x80) {
        // if new rising edge
        tempStream << unixtime_from_gps(wnR, towMsR / 1000, (long int)(sr * 1e9 + towSubMsR));
    } else {
        tempStream << ".................... ";
    }
    if (flags & 0x04) {
        // if new falling edge
        tempStream << unixtime_from_gps(wnF, towMsF / 1000, (long int)(sr * 1e9 + towSubMsF));
    } else {
        tempStream << ".................... ";
    }
    tempStream << accEst
               << " " << count
               << " " << ((flags & 0x40) >> 6)
               << " " << setfill('0') << setw(1) << ((flags & 0x18) >> 3)
               << " " << ((flags & 0x20) >> 5);
    if (verbose > 1) {
        emit toConsole(QString::fromStdString(tempStream.str()) + "\n");
    }

    struct timespec ts_r = unixtime_from_gps(wnR, towMsR / 1000, (long int)(sr * 1e9 + towSubMsR));
    struct timespec ts_f = unixtime_from_gps(wnF, towMsF / 1000, (long int)(sf * 1e9 + towSubMsF));

    struct gpsTimestamp ts;
    ts.rising_time = ts_r;
    ts.falling_time = ts_f;
    ts.valid = (flags & 0x40);
    ts.channel = ch;

    ts.counter = count;
    ts.accuracy_ns = accEst;

    ts.rising = ts.falling = false;
    if (flags & 0x80) {
        // new rising edge detected
        ts.rising = true;
    }
    if (flags & 0x04) {
        // new falling edge detected
        ts.falling = true;
    }
    emit gpsPropertyUpdatedUint32(count, eventCounter.updateAge(), 'c');
    eventCounter = count;

    static int64_t lastPulseLength = 0;

    UbxTimeMarkStruct tm;
    tm.rising = ts.rising_time;
    tm.falling = ts.falling_time;
    tm.risingValid = ts.rising;
    tm.fallingValid = ts.falling;
    tm.accuracy_ns = accEst;
    tm.valid = ts.valid;
    tm.timeBase = (flags & 0x18) >> 3;
    tm.utcAvailable = flags & 0x20;
    tm.flags = flags;
    tm.evtCounter = count;

    // try to recover the timestamp, if one edge is missing
    if (!tm.risingValid && tm.fallingValid) {
        tm.rising.tv_sec = tm.falling.tv_sec - lastPulseLength / 1000000000;
        tm.rising.tv_nsec = tm.falling.tv_nsec - lastPulseLength % 1000000000;
        if (tm.rising.tv_nsec >= 1000000000) {
            tm.rising.tv_sec += 1;
            tm.rising.tv_nsec -= 1000000000;
        } else if (tm.rising.tv_nsec < 0) {
            tm.rising.tv_sec -= 1;
            tm.rising.tv_nsec += 1000000000;
        }
    } else if (!tm.fallingValid && tm.risingValid) {
        tm.falling.tv_sec = tm.rising.tv_sec + lastPulseLength / 1000000000;
        tm.falling.tv_nsec = tm.rising.tv_nsec + lastPulseLength % 1000000000;
        if (tm.falling.tv_nsec >= 1000000000) {
            tm.falling.tv_sec += 1;
            tm.falling.tv_nsec -= 1000000000;
        }
    } else if (!tm.fallingValid && !tm.risingValid) {
        // nothing to recover here; ignore the event
    } else {
        // the normal case, i.e. both edges are valid
        // calculate the pulse length in this case
        int64_t dts = (tm.falling.tv_sec - tm.rising.tv_sec) * 1.0e9;
        dts += (tm.falling.tv_nsec - tm.rising.tv_nsec);
        if (dts > 0 && dts < 1000000)
            lastPulseLength = dts;
    }

    emit UBXReceivedTimeTM2(tm);
    return true;
}

std::vector<GnssSatellite> QtSerialUblox::UBXNavSat(const std::string& msg, bool allSats)
{
    std::vector<GnssSatellite> satList;
    // UBX-NAV-SAT: satellite information
    // parse all fields
    // GPS time of week
    uint32_t iTOW = (int)msg[0];
    iTOW += ((int)msg[1]) << 8;
    iTOW += ((int)msg[2]) << 16;
    iTOW += ((int)msg[3]) << 24;
    // version
    uint8_t version = msg[4];
    uint8_t numSvs = msg[5];

    int N = (msg.size() - 8) / 12;

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << setfill(' ') << setw(3);
        tempStream << "*** UBX-NAV-SAT message:" << endl;
        tempStream << " iTOW          : " << dec << iTOW / 1000 << " s" << endl;
        tempStream << " version       : " << dec << (int)version << endl;
        tempStream << " Nr of sats    : " << (int)numSvs << "  (nr of sections=" << N << ")\n";
        tempStream << "   Sat Data :\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    uint8_t goodSats = 0;
    for (int i = 0; i < N; i++) {
        int n = 8 + i * 12;
        uint32_t flags;

        uint8_t gnssId = msg[n + 0];
        uint8_t satId = msg[n + 1];
        uint8_t cnr = msg[n + 2];
        int8_t elev = msg[n + 3];
        int16_t azim = msg[n + 4];
        azim += msg[n + 5] << 8;
        int16_t _prRes = msg[n + 6];
        _prRes += msg[n + 7] << 8;
        float prRes = _prRes / 10.;

        flags = (int)msg[n + 8];
        flags += ((int)msg[n + 9]) << 8;
        flags += ((int)msg[n + 10]) << 16;
        flags += ((int)msg[n + 11]) << 24;
        if (gnssId > 7)
            gnssId = 7;
        GnssSatellite sat(gnssId, satId, cnr, elev, azim, prRes, flags);

        if (sat.getCnr() > 0)
            goodSats++;
        satList.push_back(sat);
    }
    if (!allSats) {
        sort(satList.begin(), satList.end(), GnssSatellite::sortByCnr);
        while (satList.back().getCnr() == 0 && satList.size() > 0)
            satList.pop_back();
    }

    emit gpsPropertyUpdatedUint8(goodSats, nrSats.updateAge(), 's');
    nrSats = goodSats;

    if (verbose > 3) {
        std::string temp;
        GnssSatellite::PrintHeader(true);
        for (vector<GnssSatellite>::iterator it = satList.begin(); it != satList.end(); it++) {
            it->Print(distance(satList.begin(), it), false);
        }
        std::stringstream tempStream;
        tempStream << "   --------------------------------------------------------------------\n";
        tempStream << " Nr of avail sats : " << (int)goodSats << "\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    return satList;
}

std::vector<GnssSatellite> QtSerialUblox::UBXNavSVinfo(const std::string& msg, bool allSats)
{
    std::vector<GnssSatellite> satList;
    // UBX-NAV-SVINFO: satellite information
    // parse all fields
    // GPS time of week
    uint32_t iTOW = (int)msg[0];
    iTOW += ((int)msg[1]) << 8;
    iTOW += ((int)msg[2]) << 16;
    iTOW += ((int)msg[3]) << 24;
    // version
    uint8_t numSvs = msg[4];
    uint8_t globFlags = msg[5];

    int N = (msg.size() - 8) / 12;

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << setfill(' ') << setw(3);
        tempStream << "*** UBX-NAV-SVINFO message:" << endl;
        tempStream << " iTOW          : " << dec << iTOW / 1000 << " s" << endl;
        tempStream << " global flags  : 0x" << hex << (int)globFlags << dec << endl;
        tempStream << " Nr of sats    : " << (int)numSvs << "  (nr of sections=" << N << ")\n";
        tempStream << "   Sat Data :\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    uint8_t goodSats = 0;
    for (int i = 0; i < N; i++) {

        int n = 8 + i * 12;
        uint8_t satId = msg[n + 1];
        uint8_t flags = msg[n + 2];
        uint8_t quality = msg[n + 3];
        uint8_t cnr = msg[n + 4];
        int8_t elev = msg[n + 5];
        int16_t azim = msg[n + 6];
        azim += msg[n + 7] << 8;
        int32_t prRes = msg[n + 8];
        prRes += msg[n + 9] << 8;
        prRes += msg[n + 10] << 16;
        prRes += msg[n + 11] << 24;

        bool used = false;
        if (flags & 0x01)
            used = true;
        uint8_t health = (flags >> 4 & 0x01);
        health += 1;
        uint8_t orbitSource = 0;
        if (flags & 0x04) {
            if (flags & 0x08)
                orbitSource = 1;
            else if (flags & 0x20)
                orbitSource = 2;
            else if (flags & 0x40)
                orbitSource = 3;
        }
        bool smoothed = (flags & 0x80);
        bool diffCorr = (flags & 0x02);
        int gnssId = 7;
        if (satId < 33)
            gnssId = 0;
        else if (satId < 65)
            gnssId = 3;
        else if (satId < 97 || satId == 255)
            gnssId = 6;
        else if (satId < 159)
            gnssId = 1;
        else if (satId < 164)
            gnssId = 3;
        else if (satId < 183)
            gnssId = 4;
        else if (satId < 198)
            gnssId = 5;
        else if (satId < 247)
            gnssId = 2;

        GnssSatellite sat(gnssId, satId, cnr, elev, azim, 0.01 * prRes,
            quality, health, orbitSource, used, diffCorr, smoothed);
        if (sat.getCnr() > 0)
            goodSats++;
        satList.push_back(sat);
    }
    if (!allSats) {
        sort(satList.begin(), satList.end(), GnssSatellite::sortByCnr);
        while (satList.back().getCnr() == 0 && satList.size() > 0)
            satList.pop_back();
    }

    emit gpsPropertyUpdatedUint8(goodSats, nrSats.updateAge(), 's');
    nrSats = goodSats;

    if (verbose > 3) {
        std::string temp;
        GnssSatellite::PrintHeader(true);
        for (vector<GnssSatellite>::iterator it = satList.begin(); it != satList.end(); it++) {
            it->Print(distance(satList.begin(), it), false);
        }
        std::stringstream tempStream;
        tempStream << "   --------------------------------------------------------------------\n";
        tempStream << " Nr of avail sats : " << goodSats << "\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    return satList;
}

void QtSerialUblox::UBXCfgGNSS(const string& msg)
{
    // UBX-CFG-GNSS: GNSS configuration
    // parse all fields
    // version
    // send:
    // "0,0,ff,1,6,5,ff,0,1,0,0,0"
    uint8_t version = msg[0];
    uint8_t numTrkChHw = msg[1];
    uint8_t numTrkChUse = msg[2];
    uint8_t numConfigBlocks = msg[3];

    int N = (msg.size() - 4) / 8;

    if (verbose > 2) {
        std::stringstream tempStream;
        tempStream << "*** UBX CFG-GNSS message:" << endl;
        tempStream << " version                    : " << dec << (int)version << endl;
        tempStream << " nr of hw tracking channels : " << dec << (int)numTrkChHw << endl;
        tempStream << " nr of channels in use      : " << dec << (int)numTrkChUse << endl;
        tempStream << " Nr of config blocks        : " << (int)numConfigBlocks
                   << "  (nr of sections=" << N << ")";
        tempStream << "  Config Data :\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    std::vector<GnssConfigStruct> configs;

    for (int i = 0; i < N; i++) {
        GnssConfigStruct config;
        config.gnssId = msg[4 + 8 * i];
        config.resTrkCh = msg[5 + 8 * i];
        config.maxTrkCh = msg[6 + 8 * i];
        config.flags = msg[8 + 8 * i];
        config.flags |= (int)msg[9 + 8 * i] << 8;
        config.flags |= (int)msg[10 + 8 * i] << 16;
        config.flags |= (int)msg[11 + 8 * i] << 24;
        if (verbose > 2) {
            std::stringstream tempStream;
            tempStream << "   " << i << ":   GNSS name : "
                       << GNSS_ID_STRING[config.gnssId] << endl;
            tempStream << "      reserved (min) tracking channels  : "
                       << dec << (int)config.resTrkCh << endl;
            tempStream << "      max nr of tracking channels used : "
                       << dec << (int)config.maxTrkCh << endl;
            tempStream << "      flags  : 0x" << std::hex << (int)config.flags << "\n";
            emit toConsole(QString::fromStdString(tempStream.str()));
        }
        configs.push_back(config);
    }
    emit UBXReceivedGnssConfig(numTrkChHw, configs);
}

void QtSerialUblox::onSetGnssConfig(const std::vector<GnssConfigStruct>& gnssConfigs)
{

    const std::size_t N = gnssConfigs.size();
    unsigned char* data { static_cast<unsigned char*>(calloc(sizeof(unsigned char), 4 + 8 * N)) };
    data[0] = 0;
    data[1] = 0;
    data[2] = 0xff;
    data[3] = static_cast<uint8_t>(N);

    for (std::size_t i { 0 }; i < N; i++) {
        uint32_t flags = gnssConfigs[i].flags;
        if (getProtVersion() >= 15.0) {
            flags |= 0x0001 << 16;
            if (gnssConfigs[i].gnssId == 5)
                flags |= 0x0004 << 16;
        }
        data[4 + 8 * i] = gnssConfigs[i].gnssId;
        data[5 + 8 * i] = gnssConfigs[i].resTrkCh;
        data[6 + 8 * i] = gnssConfigs[i].maxTrkCh;
        data[8 + 8 * i] = flags & 0xff;
        data[9 + 8 * i] = (flags >> 8) & 0xff;
        data[10 + 8 * i] = (flags >> 16) & 0xff;
        data[11 + 8 * i] = (flags >> 24) & 0xff;
    }

    enqueueMsg(UBX_CFG_GNSS, toStdString(data, static_cast<int>(4 + 8 * N)));
    free(data);
}

void QtSerialUblox::UBXCfgNav5(const string& msg)
{
    // UBX CFG-NAV5: satellite information
    // parse all fields
    uint16_t mask = msg[0];
    mask |= (int)msg[1] << 8;
    uint8_t dynModel = msg[2];
    uint8_t fixMode = msg[3];
    int32_t fixedAlt = msg[4];
    fixedAlt |= (int)msg[5] << 8;
    fixedAlt |= (int)msg[6] << 16;
    fixedAlt |= (int)msg[7] << 24;
    uint32_t fixedAltVar = msg[8];
    fixedAltVar |= (int)msg[9] << 8;
    fixedAltVar |= (int)msg[10] << 16;
    fixedAltVar |= (int)msg[11] << 24;
    int8_t minElev = msg[12];
    uint8_t cnoThreshNumSVs = msg[24];
    uint8_t cnoThresh = msg[25];

    if (verbose > 2) {
        std::stringstream tempStream;
        tempStream << "*** UBX CFG-NAV5 message:" << endl;
        tempStream << " mask               : " << std::hex << (int)mask << endl;
        tempStream << " dynamic model used : " << dec << (int)dynModel << endl;
        tempStream << " fixMode            : " << dec << (int)fixMode << endl;
        tempStream << " fixed Alt          : " << (double)fixedAlt * 0.01 << " m" << endl;
        tempStream << " fixed Alt Var      : " << (double)fixedAltVar * 0.0001 << " m^2" << endl;
        tempStream << " min elevation      : " << dec << (int)minElev << " deg\n";
        tempStream << " cnoThresh required for fix : " << dec << (int)cnoThresh << " dBHz\n";
        tempStream << " min nr of SVs having cnoThresh for fix : " << dec << (int)cnoThreshNumSVs << endl;
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}

void QtSerialUblox::setDynamicModel(uint8_t model)
{
    char buf[36];
    buf[0] = 0x01;
    buf[1] = 0x00;
    buf[2] = model; // dyn Model
    string str(buf, 36);
    enqueueMsg(UBX_CFG_NAV5, str);
}

void QtSerialUblox::UBXNavStatus(const string& msg)
{
    // UBX-NAV_STATUS: RX status information
    // parse all fields
    uint32_t iTOW = (int)msg[0];
    iTOW += ((int)msg[1]) << 8;
    iTOW += ((int)msg[2]) << 16;
    iTOW += ((int)msg[3]) << 24;

    uint8_t gpsFix = msg[4];
    uint8_t flags = msg[5];
    uint8_t flags2 = msg[7];
    uint32_t ttff = msg[8];
    ttff |= (int)msg[9] << 8;
    ttff |= (int)msg[10] << 16;
    ttff |= (int)msg[11] << 24;
    uint32_t msss = msg[12];
    msss |= (int)msg[13] << 8;
    msss |= (int)msg[14] << 16;
    msss |= (int)msg[15] << 24;

    emit gpsPropertyUpdatedUint8(gpsFix, fix.updateAge(), 'f');
    fix = gpsFix;

    emit gpsPropertyUpdatedUint32(msss / 1000, std::chrono::duration<double>(0), 'u');

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << "*** UBX NAV-STATUS message:" << endl;
        tempStream << " iTOW             : " << dec << iTOW / 1000 << " s" << endl;
        tempStream << " gpsFix           : " << dec << (int)gpsFix << endl;
        tempStream << " time to first fix: " << dec << (float)ttff / 1000. << " s" << endl;
        tempStream << " uptime           : " << dec << (float)msss / 1000. << " s" << endl;
        tempStream << " flags            : " << hex << "0x" << (int)flags << endl;
        tempStream << " flags2           : " << hex << "0x" << (int)flags2 << endl;
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}

GeodeticPos QtSerialUblox::UBXNavPosLLH(const string& msg)
{
    GeodeticPos pos;
    // GPS time of week
    uint32_t iTOW = (unsigned int)msg[0];
    iTOW += ((unsigned int)msg[1]) << 8;
    iTOW += ((unsigned int)msg[2]) << 16;
    iTOW += ((unsigned int)msg[3]) << 24;
    pos.iTOW = iTOW;
    // longitude in 1e-7 precision
    int32_t lon = (int)msg[4];
    lon += ((int)msg[5]) << 8;
    lon += ((int)msg[6]) << 16;
    lon += ((int)msg[7]) << 24;
    pos.lon = lon;
    // latitude in 1e-7 precision
    int32_t lat = (int)msg[8];
    lat += ((int)msg[9]) << 8;
    lat += ((int)msg[10]) << 16;
    lat += ((int)msg[11]) << 24;
    pos.lat = lat;
    // height above ellipsoid
    int32_t height = (int)msg[12];
    height += ((int)msg[13]) << 8;
    height += ((int)msg[14]) << 16;
    height += ((int)msg[15]) << 24;
    pos.height = height;
    // height above main sea-level
    int32_t hMSL = (int)msg[16];
    hMSL += ((int)msg[17]) << 8;
    hMSL += ((int)msg[18]) << 16;
    hMSL += ((int)msg[19]) << 24;
    pos.hMSL = hMSL;
    // horizontal accuracy estimate
    uint32_t hAcc = (unsigned int)msg[20];
    hAcc += ((unsigned int)msg[21]) << 8;
    hAcc += ((unsigned int)msg[22]) << 16;
    hAcc += ((unsigned int)msg[23]) << 24;
    pos.hAcc = hAcc;
    // vertical accuracy estimate
    uint32_t vAcc = (unsigned int)msg[24];
    vAcc += ((unsigned int)msg[25]) << 8;
    vAcc += ((unsigned int)msg[26]) << 16;
    vAcc += ((unsigned int)msg[27]) << 24;
    pos.vAcc = vAcc;
    return pos;
}

void QtSerialUblox::UBXNavClock(const std::string& msg)
{
    // parse all fields
    // GPS time of week
    uint32_t iTOW = (int)msg[0];
    iTOW += ((int)msg[1]) << 8;
    iTOW += ((int)msg[2]) << 16;
    iTOW += ((int)msg[3]) << 24;
    // clock bias
    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << "clkB[0]=" << std::setfill('0') << std::setw(2) << std::hex << (int)msg[4] << endl;
        tempStream << "clkB[1]=" << std::setfill('0') << std::setw(2) << std::hex << (int)msg[5] << endl;
        tempStream << "clkB[2]=" << std::setfill('0') << std::setw(2) << std::hex << (int)msg[6] << endl;
        tempStream << "clkB[3]=" << std::setfill('0') << std::setw(2) << std::hex << (int)msg[7];
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    int32_t clkB = (int)msg[4];
    clkB += ((int)msg[5]) << 8;
    clkB += ((int)msg[6]) << 16;
    clkB += ((int)msg[7]) << 24;
    // clock drift
    int32_t clkD = (int)msg[8];
    clkD += ((int)msg[9]) << 8;
    clkD += ((int)msg[10]) << 16;
    clkD += ((int)msg[11]) << 24;
    //mutex.lock();
    emit gpsPropertyUpdatedInt32(clkD, clkDrift.updateAge(), 'd');
    emit gpsPropertyUpdatedInt32(clkB, clkBias.updateAge(), 'b');
    clkDrift = clkD;
    clkBias = clkB;
    // time accuracy estimate
    uint32_t tAcc = (int)msg[12];
    tAcc += ((int)msg[13]) << 8;
    tAcc += ((int)msg[14]) << 16;
    tAcc += ((int)msg[15]) << 24;
    // freq accuracy estimate
    uint32_t fAcc = (int)msg[16];
    fAcc += ((int)msg[17]) << 8;
    fAcc += ((int)msg[18]) << 16;
    fAcc += ((int)msg[19]) << 24;

    emit gpsPropertyUpdatedUint32(tAcc, timeAccuracy.updateAge(), 'a');
    timeAccuracy = tAcc;
    timeAccuracy.lastUpdate = std::chrono::system_clock::now();

    emit gpsPropertyUpdatedUint32(fAcc, freqAccuracy.updateAge(), 'f');
    freqAccuracy = fAcc;
    freqAccuracy.lastUpdate = std::chrono::system_clock::now();
    // meaning of columns:
    // 01 22 - signature of NAV-CLOCK message
    // second in current week (s), clock bias (ns), clock drift (ns/s), time accuracy (ns), freq accuracy (ps/s)

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << "*** UBX-NAV-CLOCK message:" << endl;
        tempStream << " iTOW          : " << dec << iTOW / 1000 << " s" << endl;
        tempStream << " clock bias    : " << dec << clkB << " ns" << endl;
        tempStream << " clock drift   : " << dec << clkD << " ns/s" << endl;
        tempStream << " time accuracy : " << dec << tAcc << " ns" << endl;
        tempStream << " freq accuracy : " << dec << fAcc << " ps/s\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}

void QtSerialUblox::UBXNavTimeGPS(const std::string& msg)
{
    // parse all fields
    // GPS time of week
    uint32_t iTOW = (int)msg[0];
    iTOW += ((int)msg[1]) << 8;
    iTOW += ((int)msg[2]) << 16;
    iTOW += ((int)msg[3]) << 24;

    int32_t fTOW = (int)msg[4];
    fTOW += ((int)msg[5]) << 8;
    fTOW += ((int)msg[6]) << 16;
    fTOW += ((int)msg[7]) << 24;

    uint16_t wnR = (int)msg[8];
    wnR += ((int)msg[9]) << 8;

    int8_t leapS = (int)msg[10];
    uint8_t flags = (int)msg[11];

    // time accuracy estimate
    uint32_t tAcc = (int)msg[12];
    tAcc += ((int)msg[13]) << 8;
    tAcc += ((int)msg[14]) << 16;
    tAcc += ((int)msg[15]) << 24;

    double sr = iTOW / 1000.;
    sr = sr - iTOW / 1000;

    // meaning of columns:
    // 01 20 - signature of NAV-TIMEGPS message
    // week nr, second in current week, ns of timestamp in current second,
    // nr of leap seconds wrt UTC, accuracy (ns)

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << "*** UBX-NAV-TIMEGPS message:" << endl;
        tempStream << " week nr       : " << dec << wnR << endl;
        tempStream << " iTOW          : " << dec << iTOW << " ms = " << iTOW / 1000 << " s" << endl;
        tempStream << " fTOW          : " << dec << fTOW << " = " << (long int)(sr * 1e9 + fTOW) << " ns" << endl;
        tempStream << " leap seconds  : " << dec << (int)leapS << " s" << endl;
        tempStream << " time accuracy : " << dec << tAcc << " ns" << endl;
        tempStream << " flags             : ";
        for (int i = 7; i >= 0; i--)
            if (flags & 1 << i)
                tempStream << i;
            else
                tempStream << "-";
        tempStream << endl;
        tempStream << "   tow valid        : " << string((flags & 1) ? "yes" : "no") << endl;
        tempStream << "   week valid       : " << string((flags & 2) ? "yes" : "no") << endl;
        tempStream << "   leap sec valid   : " << string((flags & 4) ? "yes" : "no");
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    emit gpsPropertyUpdatedUint32(tAcc, timeAccuracy.updateAge(), 'a');
    timeAccuracy = tAcc;
    timeAccuracy.lastUpdate = std::chrono::system_clock::now();
    if (flags & 4) {
        leapSeconds = leapS;
    }

    struct timespec ts = unixtime_from_gps(wnR, iTOW / 1000, (long int)(sr * 1e9 + fTOW));
    std::stringstream tempStream;
    tempStream << ts.tv_sec << '.' << ts.tv_nsec << "\n";
    if (verbose > 1) {
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}

void QtSerialUblox::UBXNavTimeUTC(const std::string& msg)
{
    // parse all fields
    // GPS time of week
    uint32_t iTOW = (int)msg[0];
    iTOW += ((int)msg[1]) << 8;
    iTOW += ((int)msg[2]) << 16;
    iTOW += ((int)msg[3]) << 24;

    // time accuracy estimate
    uint32_t tAcc = (int)msg[4];
    tAcc += ((int)msg[5]) << 8;
    tAcc += ((int)msg[6]) << 16;
    tAcc += ((int)msg[7]) << 24;
    emit gpsPropertyUpdatedUint32(tAcc, timeAccuracy.updateAge(), 'a');
    timeAccuracy = tAcc;
    timeAccuracy.lastUpdate = std::chrono::system_clock::now();

    int32_t nano = (int)msg[8];
    nano += ((int)msg[9]) << 8;
    nano += ((int)msg[10]) << 16;
    nano += ((int)msg[11]) << 24;

    uint16_t year = (int)msg[12];
    year += ((int)msg[13]) << 8;

    uint16_t month = (int)msg[14];
    uint16_t day = (int)msg[15];
    uint16_t hour = (int)msg[16];
    uint16_t min = (int)msg[17];
    uint16_t sec = (int)msg[18];

    uint8_t flags = (int)msg[19];

    double sr = iTOW / 1000.;
    sr = sr - iTOW / 1000;

    // meaning of columns:
    // 01 21 - signature of NAV-TIMEUTC message
    // second in current week, year, month, day, hour, minute, seconds(+fraction)
    // accuracy (ns)

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << "*** UBX-NAV-TIMEUTC message:" << endl;
        tempStream << " iTOW           : " << dec << iTOW << " ms = " << iTOW / 1000 << " s" << endl;
        tempStream << " nano           : " << dec << nano << " ns" << endl;
        tempStream << " date y/m/d     : " << dec << (int)year << "/" << (int)month << "/" << (int)day << endl;
        tempStream << " UTC time h:m:s : " << dec << setw(2) << setfill('0') << hour << ":" << (int)min << ":" << (double)(sec + nano * 1e-9) << endl;
        tempStream << " time accuracy : " << dec << tAcc << " ns" << endl;
        tempStream << " flags             : ";
        for (int i = 7; i >= 0; i--)
            if (flags & 1 << i)
                tempStream << i;
            else
                tempStream << "-";
        tempStream << endl;
        tempStream << "   tow valid        : " << string((flags & 1) ? "yes" : "no") << endl;
        tempStream << "   week valid       : " << string((flags & 2) ? "yes" : "no") << endl;
        tempStream << "   UTC time valid   : " << string((flags & 4) ? "yes" : "no") << endl;
        string utcStd;
        switch ((flags & 0xf0) >> 4) {
        case 0:
            utcStd = "n/a";
            break;
        case 1:
            utcStd = "CRL";
            break;
        case 2:
            utcStd = "NIST";
            break;
        case 3:
            utcStd = "USNO";
            break;
        case 4:
            utcStd = "BIPM";
            break;
        case 5:
            utcStd = "EU";
            break;
        case 6:
            utcStd = "SU";
            break;
        case 7:
            utcStd = "NTSC";
            break;
        default:
            utcStd = "unknown";
        }
        tempStream << "   UTC standard  : " << utcStd << "\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}

void QtSerialUblox::UBXMonHW(const std::string& msg)
{
    // parse all fields
    // noise
    uint16_t noisePerMS = (int)msg[16];
    noisePerMS += ((int)msg[17]) << 8;
    noise = noisePerMS;

    // agc
    uint16_t agcCnt = (int)msg[18];
    agcCnt += ((int)msg[19]) << 8;
    agc = agcCnt;

    uint8_t antStatus = msg[20];
    uint8_t antPower = msg[21];

    uint8_t flags = (int)msg[22];

    uint8_t jamInd = msg[45];

    // meaning of columns:
    // 01 21 - signature of NAV-TIMEUTC message
    // second in current week, year, month, day, hour, minute, seconds(+fraction)
    // accuracy (ns)

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << "*** UBX-MON-HW message:" << endl;
        tempStream << " noise            : " << dec << noisePerMS << " dBc" << endl;
        tempStream << " agcCnt (0..8192) : " << dec << agcCnt << endl;
        tempStream << " antenna status   : " << dec << (int)antStatus << endl;
        tempStream << " antenna power    : " << dec << (int)antPower << endl;
        tempStream << " jamming indicator: " << dec << (int)jamInd << endl;
        tempStream << " flags             : ";
        for (int i = 7; i >= 0; i--)
            if (flags & 1 << i)
                tempStream << i;
            else
                tempStream << "-";
        tempStream << endl;
        tempStream << "   RTC calibrated   : " << string((flags & 1) ? "yes" : "no") << endl;
        tempStream << "   safe boot        : " << string((flags & 2) ? "yes" : "no") << endl;
        tempStream << "   jamming state    : " << (int)((flags & 0x0c) >> 2) << endl;
        tempStream << "   Xtal absent      : " << string((flags & 0x10) ? "yes" : "no");
        tempStream << "\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    GnssMonHwStruct hw(noisePerMS, agcCnt, antStatus, antPower, jamInd, flags);
    emit gpsMonHW(hw);
}

void QtSerialUblox::UBXMonHW2(const std::string& msg)
{
    // parse all fields
    // I/Q offset and magnitude information of front-end
    int8_t ofsI = (int8_t)msg[0];
    uint8_t magI = msg[1];
    int8_t ofsQ = (int8_t)msg[2];
    uint8_t magQ = msg[3];

    uint8_t cfgSrc = msg[4];
    uint32_t lowLevCfg = msg[8];
    lowLevCfg |= msg[9] << 8;
    lowLevCfg |= msg[10] << 16;
    lowLevCfg |= msg[11] << 24;

    uint32_t postStatus = msg[20];
    postStatus |= msg[21] << 8;
    postStatus |= msg[22] << 16;
    postStatus |= msg[23] << 24;

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << "*** UBX-MON-HW2 message:" << endl;
        tempStream << " I offset         : " << dec << (int)ofsI << endl;
        tempStream << " I magnitude      : " << dec << (int)magI << endl;
        tempStream << " Q offset         : " << dec << (int)ofsQ << endl;
        tempStream << " Q magnitude      : " << dec << (int)magQ << endl;
        tempStream << " config source    : " << hex << (int)cfgSrc << endl;
        tempStream << " POST status word : " << hex << postStatus << dec << endl;
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    GnssMonHw2Struct hw2(ofsI, ofsQ, magI, magQ, cfgSrc);
    emit gpsMonHW2(hw2);
}

void QtSerialUblox::UBXMonVer(const std::string& msg)
{
    // parse all fields
    std::string hwString = "";
    std::string swString = "";

    for (int i = 0; msg[i] != 0 && i < 30; i++) {
        swString += (char)msg[i];
    }
    for (int i = 30; msg[i] != 0 && i < 40; i++) {
        hwString += (char)msg[i];
    }

    if (verbose > 2) {
        std::stringstream tempStream;
        tempStream << "*** UBX-MON-VER message:" << endl;
        tempStream << " sw version  : " << swString << endl;
        tempStream << " hw version  : " << hwString << endl;
        emit toConsole(QString::fromStdString(tempStream.str()));
    }

    std::vector<std::string> result;
    std::string::size_type i = 0;
    while (i != std::string::npos && i < msg.size()) {
        std::string s = msg.substr(i, msg.find((char)0x00, i + 1) - i + 1);
        while (s.size() && s[0] == 0x00) {
            s.erase(0, 1);
        }
        if (s.size()) {
            result.push_back(s);
            size_t n = s.find("PROTVER", 0);
            if (n != std::string::npos) {
                std::string str = s.substr(7, s.size() - 7);
                while (str.size() && !isdigit(str[0]))
                    str.erase(0, 1);
                while (str.size() && (str[str.size() - 1] == ' ' || !std::isgraph(static_cast<unsigned char>(str[str.size() - 1]))))
                    str.erase(str.size() - 1, 1);
                fProtVersionString = str;
                if (verbose > 3)
                    emit toConsole("catched PROTVER string: '" + QString::fromStdString(fProtVersionString) + "'\n");
                float nr = QtSerialUblox::getProtVersion();
                if (verbose > 3)
                    emit toConsole("ver: " + QString::number(nr) + "\n");
            }
            if (verbose > 2) {
                emit toConsole(QString::fromStdString(s) + "\n");
            }
        }
        i = msg.find((char)0x00, i + 1);
    }
    emit gpsVersion(QString::fromStdString(swString), QString::fromStdString(hwString), QString::fromStdString(fProtVersionString));
}

void QtSerialUblox::UBXMonTx(const std::string& msg)
{
    // parse all fields
    // nr bytes pending
    uint16_t pending[6];
    uint8_t usage[6];
    uint8_t peakUsage[6];
    uint8_t tUsage;
    uint8_t tPeakUsage;

    for (int i = 0; i < 6; i++) {
        pending[i] = msg[2 * i];
        pending[i] |= (uint16_t)msg[2 * i + 1] << 8;
        usage[i] = msg[i + 12];
        peakUsage[i] = msg[i + 18];
    }

    tUsage = msg[24];
    tPeakUsage = msg[25];

    // meaning of columns:
    // 01 21 - signature of NAV-TIMEUTC message
    // second in current week, year, month, day, hour, minute, seconds(+fraction)
    // accuracy (ns)
    emit UBXReceivedTxBuf(tUsage, tPeakUsage);
    txBufUsage = tUsage;
    txBufPeakUsage = tPeakUsage;

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << setfill(' ') << setw(3);
        tempStream << "*** UBX-MON-TXBUF message:" << endl;
        tempStream << " global TX buf usage      : " << dec << (int)tUsage << " %" << endl;
        tempStream << " global TX buf peak usage : " << dec << (int)tPeakUsage << " %" << endl;
        tempStream << " TX buf usage for target      : ";
        for (int i = 0; i < 6; i++) {
            tempStream << "    (" << dec << i << ") " << setw(3) << (int)usage[i];
        }
        tempStream << endl;
        tempStream << " TX buf peak usage for target : ";
        for (int i = 0; i < 6; i++) {
            tempStream << "    (" << dec << i << ") " << setw(3) << (int)peakUsage[i];
        }
        tempStream << endl;
        tempStream << " TX bytes pending for target  : ";
        for (int i = 0; i < 6; i++) {
            tempStream << "    (" << dec << i << ") " << setw(3) << pending[i];
        }
        tempStream << "\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}

void QtSerialUblox::UBXMonRx(const std::string& msg)
{
    // parse all fields
    // nr bytes pending
    uint16_t pending[6];
    uint8_t usage[6];
    uint8_t peakUsage[6];
    uint8_t tUsage = 0;
    uint8_t tPeakUsage = 0;

    for (int i = 0; i < 6; i++) {
        pending[i] = msg[2 * i];
        pending[i] |= (uint16_t)msg[2 * i + 1] << 8;
        usage[i] = msg[i + 12];
        peakUsage[i] = msg[i + 18];
        tUsage += usage[i];
        tPeakUsage += peakUsage[i];
    }

    emit UBXReceivedRxBuf(tUsage, tPeakUsage);

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << setfill(' ') << setw(3);
        tempStream << "*** UBX-MON-RXBUF message:" << endl;
        tempStream << " global RX buf usage      : " << dec << (int)tUsage << " %" << endl;
        tempStream << " global RX buf peak usage : " << dec << (int)tPeakUsage << " %" << endl;
        tempStream << " RX buf usage for target      : ";
        for (int i = 0; i < 6; i++) {
            tempStream << "    (" << dec << i << ") " << setw(3) << (int)usage[i];
        }
        tempStream << endl;
        tempStream << " RX buf peak usage for target : ";
        for (int i = 0; i < 6; i++) {
            tempStream << "    (" << dec << i << ") " << setw(3) << (int)peakUsage[i];
        }
        tempStream << endl;
        tempStream << " RX bytes pending for target  : ";
        for (int i = 0; i < 6; i++) {
            tempStream << "    (" << dec << i << ") " << setw(3) << pending[i];
        }
        tempStream << "\n";
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}

void QtSerialUblox::UBXCfgNavX5(const std::string& msg)
{
    // parse all fields
    uint8_t version = msg[0];
    uint16_t mask1 = msg[1];
    mask1 |= msg[2] << 8;
    uint8_t minSVs = msg[10];
    uint8_t maxSVs = msg[11];
    uint8_t minCNO = msg[12];
    uint8_t iniFix3D = msg[14];
    uint16_t wknRollover = msg[18];
    wknRollover |= msg[19] << 8;
    uint8_t aopCfg = msg[27];
    uint16_t aopOrbMaxErr = msg[30];
    aopOrbMaxErr |= msg[31] << 8;
    if (verbose > 2) {
        std::stringstream tempStream;
        tempStream << "*** UBX-MON-NAVX5 message:" << endl;
        tempStream << " msg version         : " << dec << (int)version << endl;
        tempStream << " min nr of SVs       : " << dec << (int)minSVs << endl;
        tempStream << " max nr of SVs       : " << dec << (int)maxSVs << endl;
        tempStream << " min CNR for nav     : " << dec << (int)minCNO << endl;
        tempStream << " initial 3D fix      : " << dec << (int)iniFix3D << endl;
        tempStream << " GPS week rollover   : " << dec << (int)wknRollover << endl;
        tempStream << " AOP auton config    : " << dec << (int)aopCfg << endl;
        tempStream << " max AOP orbit error : " << (int)aopOrbMaxErr << " m" << endl;
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}

void QtSerialUblox::UBXCfgAnt(const std::string& msg)
{
    // parse all fields
    uint16_t flags = msg[0];
    flags |= msg[1] << 8;
    uint16_t pins = msg[2];
    pins |= msg[3] << 8;
    if (verbose > 2) {
        std::stringstream tempStream;
        tempStream << "*** UBX-CFG-ANT message:" << endl;
        tempStream << " flags                     : 0x" << hex << (int)flags << dec << endl;
        tempStream << " ant supply control signal : " << string((flags & 0x01) ? "on" : "off") << endl;
        tempStream << " short detection           : " << string((flags & 0x02) ? "on" : "off") << endl;
        tempStream << " open detection            : " << string((flags & 0x04) ? "on" : "off") << endl;
        tempStream << " pwr down on short         : " << string((flags & 0x08) ? "on" : "off") << endl;
        tempStream << " auto recovery from short  : " << string((flags & 0x10) ? "on" : "off") << endl;
        tempStream << " supply switch pin         : " << (int)(pins & 0x1f) << endl;
        tempStream << " short detection pin       : " << (int)((pins >> 5) & 0x1f) << endl;
        tempStream << " open detection pin        : " << (int)((pins >> 10) & 0x1f) << endl;

        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}

void QtSerialUblox::UBXCfgTP5(const std::string& msg)
{
    UbxTimePulseStruct tp;
    // parse all fields
    tp.tpIndex = msg[0];
    tp.version = msg[1];
    tp.antCableDelay = msg[4];
    tp.antCableDelay |= msg[5] << 8;
    tp.rfGroupDelay = msg[6];
    tp.rfGroupDelay |= msg[7] << 8;
    tp.freqPeriod = msg[8];
    tp.freqPeriod |= msg[9] << 8;
    tp.freqPeriod |= msg[10] << 16;
    tp.freqPeriod |= msg[11] << 24;
    tp.freqPeriodLock = msg[12];
    tp.freqPeriodLock |= msg[13] << 8;
    tp.freqPeriodLock |= msg[14] << 16;
    tp.freqPeriodLock |= msg[15] << 24;
    tp.pulseLenRatio = msg[16];
    tp.pulseLenRatio |= msg[17] << 8;
    tp.pulseLenRatio |= msg[18] << 16;
    tp.pulseLenRatio |= msg[19] << 24;
    tp.pulseLenRatioLock = msg[20];
    tp.pulseLenRatioLock |= msg[21] << 8;
    tp.pulseLenRatioLock |= msg[22] << 16;
    tp.pulseLenRatioLock |= msg[23] << 24;
    tp.userConfigDelay = msg[24];
    tp.userConfigDelay |= msg[25] << 8;
    tp.userConfigDelay |= msg[26] << 16;
    tp.userConfigDelay |= msg[27] << 24;
    tp.flags = msg[28];
    tp.flags |= msg[29] << 8;
    tp.flags |= msg[30] << 16;
    tp.flags |= msg[31] << 24;
    bool isFreq = tp.flags & 0x08;
    bool isLength = tp.flags & 0x10;

    if (verbose > 2) {
        std::stringstream tempStream;
        tempStream << "*** UBX-CFG-TP5 message:" << endl;
        tempStream << " message version           : " << dec << (int)tp.version << endl;
        tempStream << " time pulse index          : " << dec << (int)tp.tpIndex << endl;
        tempStream << " ant cable delay           : " << dec << (int)tp.antCableDelay << " ns" << endl;
        tempStream << " rf group delay            : " << dec << (int)tp.rfGroupDelay << " ns" << endl;
        tempStream << " user config delay         : " << dec << (int)tp.userConfigDelay << " ns" << endl;
        if (isFreq) {
            tempStream << " pulse frequency           : " << dec << (int)tp.freqPeriod << " Hz" << endl;
            tempStream << " locked pulse frequency    : " << dec << (int)tp.freqPeriodLock << " Hz" << endl;
        } else {
            tempStream << " pulse period              : " << dec << (int)tp.freqPeriod << " us" << endl;
            tempStream << " locked pulse period       : " << dec << (int)tp.freqPeriodLock << " us" << endl;
        }
        if (isLength) {
            tempStream << " pulse length              : " << dec << (int)tp.pulseLenRatio << " us" << endl;
            tempStream << " locked pulse length       : " << dec << (int)tp.pulseLenRatioLock << " us" << endl;
        } else {
            tempStream << " pulse duty cycle          : " << dec << (double)tp.pulseLenRatio / ((uint64_t)1 << 32) << endl;
            tempStream << " locked pulse duty cycle   : " << dec << (double)tp.pulseLenRatioLock / ((uint64_t)1 << 32) << endl;
        }
        tempStream << " flags                     : 0x" << hex << (int)tp.flags << dec << endl;
        tempStream << " tp active                 : " << string((tp.flags & 0x01) ? "yes" : "no") << endl;

        tempStream << " lockGpsFreq               : " << string((tp.flags & 0x02) ? "on" : "off") << endl;
        tempStream << " lockedOtherSet            : " << string((tp.flags & 0x04) ? "on" : "off") << endl;
        tempStream << " isFreq                    : " << string((tp.flags & 0x08) ? "on" : "off") << endl;
        tempStream << " isLength                  : " << string((tp.flags & 0x10) ? "on" : "off") << endl;
        tempStream << " alignToTow                : " << string((tp.flags & 0x20) ? "on" : "off") << endl;
        tempStream << " polarity                  : " << string((tp.flags & 0x40) ? "rising" : "falling") << endl;
        tempStream << " time grid                 : ";
        if (getProtVersion() < 16)
            tempStream << string((tp.flags & 0x80) ? "GPS" : "UTC") << endl;
        else {
            int timeGrid = (tp.flags & UbxTimePulseStruct::GRID_UTC_GPS) >> 7;
            switch (timeGrid) {
            case 0:
                tempStream << "UTC" << endl;
                break;
            case 1:
                tempStream << "GPS" << endl;
                break;
            case 2:
                tempStream << "Glonass" << endl;
                break;
            case 3:
                tempStream << "BeiDou" << endl;
                break;
            case 4:
                tempStream << "Galileo" << endl;
                break;
            default:
                tempStream << "unknown" << endl;
            }
        }

        emit toConsole(QString::fromStdString(tempStream.str()));
    }
    emit UBXReceivedTP5(tp);
}

void QtSerialUblox::UBXSetCfgTP5(const UbxTimePulseStruct& tp)
{
    unsigned char msg[32];

    msg[0] = tp.tpIndex;
    msg[1] = 0;
    msg[4] = tp.antCableDelay & 0xff;
    msg[5] = (tp.antCableDelay >> 8) & 0xff;
    msg[6] = tp.rfGroupDelay & 0xff;
    msg[7] = (tp.rfGroupDelay >> 8) & 0xff;
    msg[8] = tp.freqPeriod & 0xff;
    msg[9] = (tp.freqPeriod >> 8) & 0xff;
    msg[10] = (tp.freqPeriod >> 16) & 0xff;
    msg[11] = (tp.freqPeriod >> 24) & 0xff;
    msg[12] = tp.freqPeriodLock & 0xff;
    msg[13] = (tp.freqPeriodLock >> 8) & 0xff;
    msg[14] = (tp.freqPeriodLock >> 16) & 0xff;
    msg[15] = (tp.freqPeriodLock >> 24) & 0xff;
    msg[16] = tp.pulseLenRatio & 0xff;
    msg[17] = (tp.pulseLenRatio >> 8) & 0xff;
    msg[18] = (tp.pulseLenRatio >> 16) & 0xff;
    msg[19] = (tp.pulseLenRatio >> 24) & 0xff;
    msg[20] = tp.pulseLenRatioLock & 0xff;
    msg[21] = (tp.pulseLenRatioLock >> 8) & 0xff;
    msg[22] = (tp.pulseLenRatioLock >> 16) & 0xff;
    msg[23] = (tp.pulseLenRatioLock >> 24) & 0xff;
    msg[24] = tp.userConfigDelay & 0xff;
    msg[25] = (tp.userConfigDelay >> 8) & 0xff;
    msg[26] = (tp.userConfigDelay >> 16) & 0xff;
    msg[27] = (tp.userConfigDelay >> 24) & 0xff;
    msg[28] = tp.flags & 0xff;
    msg[29] = (tp.flags >> 8) & 0xff;
    msg[30] = (tp.flags >> 16) & 0xff;
    msg[31] = (tp.flags >> 24) & 0xff;

    enqueueMsg(UBX_CFG_TP5, toStdString(msg, 32));
}

void QtSerialUblox::UBXNavDOP(const string& msg)
{
    // UBX-NAV-DOP: dilution of precision values
    UbxDopStruct d;

    // parse all fields
    uint32_t iTOW = (int)msg[0];
    iTOW += ((int)msg[1]) << 8;
    iTOW += ((int)msg[2]) << 16;
    iTOW += ((int)msg[3]) << 24;

    // geometric DOP
    d.gDOP = msg[4];
    d.gDOP |= msg[5] << 8;
    // position DOP
    d.pDOP = msg[6];
    d.pDOP |= msg[7] << 8;
    // time DOP
    d.tDOP = msg[8];
    d.tDOP |= msg[9] << 8;
    // vertical DOP
    d.vDOP = msg[10];
    d.vDOP |= msg[11] << 8;
    // horizontal DOP
    d.hDOP = msg[12];
    d.hDOP |= msg[13] << 8;
    // northing DOP
    d.nDOP = msg[14];
    d.nDOP |= msg[15] << 8;
    // easting DOP
    d.eDOP = msg[16];
    d.eDOP |= msg[17] << 8;

    emit UBXReceivedDops(d);

    if (verbose > 3) {
        std::stringstream tempStream;
        tempStream << "*** UBX NAV-DOP message:" << endl;
        tempStream << " iTOW             : " << dec << iTOW / 1000 << " s" << endl;
        tempStream << " geometric DOP    : " << dec << d.gDOP / 100. << endl;
        tempStream << " position DOP     : " << dec << d.pDOP / 100. << endl;
        tempStream << " time DOP         : " << dec << d.tDOP / 100. << endl;
        tempStream << " vertical DOP     : " << dec << d.vDOP / 100. << endl;
        tempStream << " horizontal DOP   : " << dec << d.hDOP / 100. << endl;
        tempStream << " northing DOP     : " << dec << d.nDOP / 100. << endl;
        tempStream << " easting DOP      : " << dec << d.eDOP / 100. << endl;
        emit toConsole(QString::fromStdString(tempStream.str()));
    }
}
