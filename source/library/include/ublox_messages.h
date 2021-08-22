#ifndef UBLOX_MESSAGES_H
#define UBLOX_MESSAGES_H

#include "muondetector_shared_global.h"

#define UBLOX_VERSION 7
// not in this list are all msg of types: RXM, LOG, AID and INF

// list of UBX message Cls/ID
#define UBX_ACK 0x0501
#define UBX_NAK 0x0500

#define UBX_NAV_CLOCK 0x0122
#define UBX_NAV_DGPS 0x0131
#define UBX_NAV_AOPSTATUS 0x0160
#define UBX_NAV_DOP 0x0104
#define UBX_NAV_EOE 0x0161 // not supportet on U-Blox 7
#define UBX_NAV_GEOFENCE 0x0139 // not supportet on U-Blox 7
#define UBX_NAV_ODO 0x0109 // not supportet on U-Blox 7
#define UBX_NAV_ORB 0x0134 // not supportet on U-Blox 7
#define UBX_NAV_POSECEF 0x0101
#define UBX_NAV_POSLLH 0x0102
#define UBX_NAV_PVT 0x0107
#define UBX_NAV_RESETODO 0x0110 // not supportet on U-Blox 7
#define UBX_NAV_SAT 0x0135 // not supportet on U-Blox 7
#define UBX_NAV_SBAS 0x0132
#define UBX_NAV_SOL 0x0106
#define UBX_NAV_STATUS 0x0103
#define UBX_NAV_SVINFO 0x0130
#define UBX_NAV_TIMEBDS 0x0124 // not supportet on U-Blox 7
#define UBX_NAV_TIMEGAL 0x0125 // not supportet on U-Blox 7
#define UBX_NAV_TIMEGLO 0x0123 // not supportet on U-Blox 7
#define UBX_NAV_TIMEGPS 0x0120
#define UBX_NAV_TIMELS 0x0126 // not supportet on U-Blox 7
#define UBX_NAV_TIMEUTC 0x0121
#define UBX_NAV_VELECEF 0x0111
#define UBX_NAV_VELNED 0x0112

#define UBX_CFG_ANT 0x0613
#define UBX_CFG_CFG 0x0609
#define UBX_CFG_DAT 0x0606
#define UBX_CFG_DOSC 0x0661 // not supportet on U-Blox 7 (only with time & frequency sync products)
#define UBX_CFG_DYNSEED 0x0685 // not supportet on U-Blox 7
#define UBX_CFG_ESRC 0x0660 // not supportet on U-Blox 7 (only with time & frequency sync products)
#define UBX_CFG_FIXSEED 0x0684 // not supportet on U-Blox 7
#define UBX_CFG_GEOFENCE 0x0669 // not supportet on U-Blox 7
#define UBX_CFG_GNSS 0x063e
#define UBX_CFG_INF 0x0602
#define UBX_CFG_ITFM 0x0639
#define UBX_CFG_LOGFILTER 0x0647
#define UBX_CFG_MSG 0x0601
#define UBX_CFG_NAV5 0x0624
#define UBX_CFG_NAVX5 0x0623
#define UBX_CFG_NMEA 0x0617
#define UBX_CFG_ODO 0x061e // not supportet on U-Blox 7
#define UBX_CFG_PM2 0x063b
#define UBX_CFG_PMS 0x0686 // not supportet on U-Blox 7
#define UBX_CFG_PRT 0x0600
#define UBX_CFG_PWR 0x0657 // not supportet on U-Blox 7
#define UBX_CFG_RATE 0x0608
#define UBX_CFG_RINV 0x0634
#define UBX_CFG_RST 0x0604
#define UBX_CFG_RXM 0x0611
#define UBX_CFG_SBAS 0x0616
#define UBX_CFG_SMGR 0x0662 // not supportet on U-Blox 7 (only with time & frequency sync products)
#define UBX_CFG_TMODE2 0x063d // not supportet on U-Blox 7 (only for timing receivers)
#define UBX_CFG_TP5 0x0631
#define UBX_CFG_TXSLOT 0x0653 // not supportet on U-Blox 7 (only with time & frequency sync products)
#define UBX_CFG_USB 0x061b

#define UBX_TIM_TP 0x0d01
#define UBX_TIM_TM2 0x0d03
#define UBX_TIM_VRFY 0x0d06

#define UBX_MON_VER 0x0a04
#define UBX_MON_GNSS 0x0a28 // not supportet on U-Blox 7
#define UBX_MON_HW 0x0a09
#define UBX_MON_HW2 0x0a0b
#define UBX_MON_IO 0x0a02
#define UBX_MON_MSGPP 0x0a06
#define UBX_MON_PATCH 0x0a27 // not supportet on U-Blox 7
#define UBX_MON_RXBUF 0x0a07
#define UBX_MON_RXR 0x0a21
#define UBX_MON_SMGR 0x0a2e // not supportet on U-Blox 7 (only with time & frequency sync products)
#define UBX_MON_TXBUF 0x0a08

// list of NMEA message Cls/ID
#define UBX_NMEA_DTM 0xf00a
#define UBX_NMEA_GBQ 0xf044
#define UBX_NMEA_GBS 0xf009
#define UBX_NMEA_GGA 0xf000
#define UBX_NMEA_GLL 0xf001
#define UBX_NMEA_GLQ 0xf043
#define UBX_NMEA_GNQ 0xf042
#define UBX_NMEA_GNS 0xf00d
#define UBX_NMEA_GPQ 0xf040
#define UBX_NMEA_GRS 0xf006
#define UBX_NMEA_GSA 0xf002
#define UBX_NMEA_GST 0xf007
#define UBX_NMEA_GSV 0xf003
#define UBX_NMEA_RMC 0xf004
#define UBX_NMEA_TXT 0xf041
#define UBX_NMEA_VLW 0xf00f
#define UBX_NMEA_VTG 0xf005
#define UBX_NMEA_ZDA 0xf008
#define UBX_NMEA_CONFIG 0xf141
#define UBX_NMEA_POSITION 0xf100
#define UBX_NMEA_RATE 0xf140
#define UBX_NMEA_SVSTATUS 0xf103
#define UBX_NMEA_TIME 0xf104

// proto is the shortcut for protocol and
// is defined as the correct bitmask for one
// protocol
#define PROTO_UBX 1
#define PROTO_NMEA 0b10
#define PROTO_RTCM3 0b100000
#endif // UBLOX_MESSAGES_H
