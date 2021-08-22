#ifndef UBLOX_STRUCTS_H
#define UBLOX_STRUCTS_H

#include <chrono>
#include <sstream>
#include <string>

#include "muondetector_structs.h"

struct GeodeticPos;
class GnssSatellite;

struct UbxMessage {
public:
    uint16_t msgID;
    std::string data;
};

struct gpsTimestamp {
    struct timespec rising_time;
    struct timespec falling_time;
    double accuracy_ns;
    bool valid;
    int channel;
    bool rising;
    bool falling;
    int counter;
};

template <typename T>
class gpsProperty {
public:
    gpsProperty()
        : value()
    {
        updated = false;
    }
    gpsProperty(const T& val)
    {
        value = val;
        updated = true;
        lastUpdate = std::chrono::system_clock::now();
    }
    std::chrono::time_point<std::chrono::system_clock> lastUpdate;
    std::chrono::duration<double> updatePeriod;
    std::chrono::duration<double> updateAge() { return std::chrono::system_clock::now() - lastUpdate; }
    bool updated;
    gpsProperty& operator=(const T& val)
    {
        value = val;
        lastUpdate = std::chrono::system_clock::now();
        updated = true;
        return *this;
    }
    const T& operator()()
    {
        updated = false;
        return value;
    }

private:
    T value;
};

struct UbxDopStruct {
    uint16_t gDOP = 0, pDOP = 0, tDOP = 0, vDOP = 0, hDOP = 0, nDOP = 0, eDOP = 0;
};

#endif // UBLOX_STRUCTS_H
