#include "platform/clock_hal.hpp"

// Заглушка для реального таймера МК
namespace platform {

uint32_t now_ms() {
    static uint32_t fake_ticks = 0;
    return fake_ticks++;
}

} // namespace platform
