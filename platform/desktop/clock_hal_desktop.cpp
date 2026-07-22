#include "platform/clock_hal.hpp"

#include <chrono>
#include <thread>

namespace platform {

uint32_t now_ms() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void sleep_ms(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace platform
