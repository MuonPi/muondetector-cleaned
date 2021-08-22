#ifndef CUSTOM_IO_OPERATORS_H
#define CUSTOM_IO_OPERATORS_H
#include <QString>
#include <chrono>
#include <iomanip>
#include <iostream>

std::ostream& operator<<(std::ostream& os, const QString& someQString);
std::ostream& operator<<(std::ostream& os, const std::chrono::time_point<std::chrono::system_clock>& timestamp);
std::ostream& operator<<(std::ostream& os, const timespec& ts);

#endif //CUSTOM_IO_OPERATORS_H
