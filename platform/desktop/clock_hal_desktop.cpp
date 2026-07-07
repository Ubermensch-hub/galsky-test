#include "platform/clock_hal.hpp"

#include <chrono>

namespace platform {

uint32_t now_ms() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace platform
