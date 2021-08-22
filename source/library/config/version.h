#ifndef MUONDETECTOR_VERSION_H
#define MUONDETECTOR_VERSION_H

namespace MuonPi::CMake::Version {
constexpr int major { @PROJECT_VERSION_MAJOR@ };
constexpr int minor { @PROJECT_VERSION_MINOR@ };
constexpr int patch { @PROJECT_VERSION_PATCH@ };
constexpr const char* additional { "@PROJECT_VERSION_ADDITIONAL@" };
constexpr const char* hash { "@PROJECT_VERSION_HASH@" };
}

#endif // MUONDETECTOR_VERSION_H
