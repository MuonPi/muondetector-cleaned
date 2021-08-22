#ifndef GEOHASH_H
#define GEOHASH_H

#include <QObject>
#include <QString>

class GeoHash : public QObject {
    Q_OBJECT
public:
    GeoHash() = delete;
    static QString hashFromCoordinates(double lon, double lat, int precision = 6);
};

#endif // GEOHASH_H
