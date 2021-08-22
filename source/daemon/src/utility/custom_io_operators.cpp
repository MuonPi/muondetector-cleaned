#include "utility/custom_io_operators.h"

std::ostream& operator<<(std::ostream& os, const QString& someQString)
{
    os << someQString.toStdString();
    return os;
}

std::ostream& operator<<(std::ostream& os, const std::chrono::time_point<std::chrono::system_clock>& timestamp)
{
    std::chrono::microseconds mus = std::chrono::duration_cast<std::chrono::microseconds>(timestamp.time_since_epoch());
    std::chrono::seconds secs = std::chrono::duration_cast<std::chrono::seconds>(mus);
    std::chrono::microseconds subs = mus - secs;

    os << secs.count() << "." << std::setw(6) << std::setfill('0') << subs.count() << " " << std::setfill(' ');
    return os;
}

std::ostream& operator<<(std::ostream& os, const timespec& ts)
{
    os << ts.tv_sec << "." << std::setw(9) << std::setfill('0') << ts.tv_nsec << " " << std::setfill(' ');
    return os;
}
