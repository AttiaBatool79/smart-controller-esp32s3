#pragma once
// One JSON snapshot of the whole device. Used by the web API and MQTT,
// so both always report the same thing.
#include <string>

namespace status {
std::string Json();
}
