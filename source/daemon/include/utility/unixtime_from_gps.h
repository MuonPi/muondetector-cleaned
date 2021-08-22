#ifndef UNIXTIME_FROM_GPS_H
#define UNIXTIME_FROM_GPS_H

#include <chrono>
#include <QDateTime>

static timespec unixtime_from_gps(int week_nr, long int s_of_week, long int ns) {
    QDateTime gpsTimeStart(QDate(1980,1,6),QTime(0,0,0,0),Qt::UTC);
    quint64 gpsOffsetToEpoch = gpsTimeStart.toMSecsSinceEpoch()/1000;
    quint64 secs = gpsOffsetToEpoch;
    secs += week_nr*7*24*3600;  // 7 days per week, 24 hours per day and 3600 seconds per hour
    secs += s_of_week;
    struct timespec ts;
    ts.tv_sec = secs;
    ts.tv_nsec = ns;

    return ts;
}

#endif // UNIXTIME_FROM_GPS_H
