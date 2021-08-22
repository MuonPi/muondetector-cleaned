#include "utility/gpio_mapping.h"
#include "config.h"
#include <map>

std::map<GPIO_PIN, unsigned int> GPIO_PINMAP = GPIO_PINMAP_VERSIONS[MuonPi::Version::hardware.major];
