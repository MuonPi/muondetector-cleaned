#ifndef UBX_UBX_KEY_NAME_MAP_H
#define UBX_UBX_KEY_NAME_MAP_H

#include "muondetector_shared_global.h"
#include "ublox_messages.h"

#include <QMap>
#include <QString>
#include <ublox_messages.h>

const QMap<short unsigned int, QString> ubxMsgKeyNameMap({ { UBX_ACK, "ACK-ACK" },
    { UBX_NAK, "ACK-NAK" },
    // NAV
    { UBX_NAV_CLOCK, "NAV-CLOCK" },
    { UBX_NAV_DGPS, "NAV-DGPS" },
    { UBX_NAV_AOPSTATUS, "NAV-AOPSTATUS" },
    { UBX_NAV_DOP, "NAV-DOP" },
    { UBX_NAV_POSECEF, "NAV-POSECEF" },
    { UBX_NAV_POSLLH, "NAV-POSLLH" },
    { UBX_NAV_PVT, "NAV-PVT" },
    { UBX_NAV_SBAS, "NAV-SBAS" },
    { UBX_NAV_SOL, "NAV-SOL" },
    { UBX_NAV_STATUS, "NAV-STATUS" },
    { UBX_NAV_SVINFO, "NAV-SVINFO" },
    { UBX_NAV_TIMEGPS, "NAV-TIMEGPS" },
    { UBX_NAV_TIMEUTC, "NAV-TIMEUTC" },
    { UBX_NAV_VELECEF, "NAV-VELECEF" },
    { UBX_NAV_VELNED, "NAV-VELNED" },
    // CFG
    { UBX_CFG_ANT, "CFG-ANT" },
    { UBX_CFG_CFG, "CFG-CFG" },
    { UBX_CFG_DAT, "CFG-DAT" },
    { UBX_CFG_GNSS, "CFG-GNSS" },
    { UBX_CFG_INF, "CFG-INF" },
    { UBX_CFG_ITFM, "CFG-ITFM" },
    { UBX_CFG_LOGFILTER, "CFG-LOGFILTER" },
    { UBX_CFG_MSG, "CFG-MSG" },
    { UBX_CFG_NAV5, "CFG-NAV5" },
    { UBX_CFG_NAVX5, "CFG-NAV5X" },
    { UBX_CFG_NMEA, "CFG-NMEA" },
    { UBX_CFG_PM2, "CFG-PM2" },
    { UBX_CFG_PRT, "CFG-PRT" },
    { UBX_CFG_RATE, "CFG-RATE" },
    { UBX_CFG_RINV, "CFG-RINV" },
    { UBX_CFG_RST, "CFG-RST" },
    { UBX_CFG_RXM, "CFG-RXM" },
    { UBX_CFG_SBAS, "CFG-SBAS" },
    { UBX_CFG_TP5, "CFG-TP5" },
    { UBX_CFG_USB, "CFG-USB" },
    // TIM
    { UBX_TIM_TP, "TIM-TP" },
    { UBX_TIM_TM2, "TIM-TM2" },
    { UBX_TIM_VRFY, "TIM-VRFY" },
    // MON
    { UBX_MON_VER, "MON-VER" },
    { UBX_MON_HW, "MON-HW" },
    { UBX_MON_HW2, "MON-HW2" },
    { UBX_MON_IO, "MON-IO" },
    { UBX_MON_MSGPP, "MON-MSGP" },
    { UBX_MON_RXBUF, "MON-RXBUF" },
    { UBX_MON_RXR, "MON-RXR" },
    { UBX_MON_TXBUF, "MON-TXBUF" },

    // the messages only used in Ublox-8
    { UBX_NAV_EOE, "NAV-EOE" },
    { UBX_NAV_GEOFENCE, "NAV-GEOFENCE" },
    { UBX_NAV_ODO, "NAV-ODO" },
    { UBX_NAV_ORB, "NAV-ORB" },
    { UBX_NAV_RESETODO, "NAV-RESETODO" },
    { UBX_NAV_SAT, "NAV-SAT" },
    { UBX_NAV_TIMEBDS, "NAV-TIMEBDS" },
    { UBX_NAV_TIMEGAL, "NAV-TIMEGAL" },
    { UBX_NAV_TIMEGLO, "NAV-TIMEGLO" },
    { UBX_NAV_TIMELS, "NAV-TIMELS" },

    // NMEA
    { UBX_NMEA_DTM, "NMEA-DTM" },
    { UBX_NMEA_GBQ, "NMEA-GBQ" },
    { UBX_NMEA_GBS, "NMEA-GBS" },
    { UBX_NMEA_GGA, "NMEA-GGA" },
    { UBX_NMEA_GLL, "NMEA-GLL" },
    { UBX_NMEA_GLQ, "NMEA-GLQ" },
    { UBX_NMEA_GNQ, "NMEA-GNQ" },
    { UBX_NMEA_GNS, "NMEA-GNS" },
    { UBX_NMEA_GPQ, "NMEA-GPQ" },
    { UBX_NMEA_GRS, "NMEA-GRS" },
    { UBX_NMEA_GSA, "NMEA-GSA" },
    { UBX_NMEA_GST, "NMEA-GST" },
    { UBX_NMEA_GSV, "NMEA-GSV" },
    { UBX_NMEA_RMC, "NMEA-RMC" },
    { UBX_NMEA_TXT, "NMEA-TXT" },
    { UBX_NMEA_VLW, "NMEA-VLW" },
    { UBX_NMEA_VTG, "NMEA-VTG" },
    { UBX_NMEA_ZDA, "NMEA-ZDA" },
    { UBX_NMEA_CONFIG, "NMEA-CONFIG" },
    { UBX_NMEA_POSITION, "NMEA-POSITION" },
    { UBX_NMEA_RATE, "NMEA-RATE" },
    { UBX_NMEA_SVSTATUS, "NMEA-SVSTATUS" },
    { UBX_NMEA_TIME, "NMEA-TIME" } });
#endif // UBX_UBX_KEY_NAME_MAP_H
