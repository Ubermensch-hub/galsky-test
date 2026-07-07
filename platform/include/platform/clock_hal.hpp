#pragma once

#include <cstdint>

// Доступ к счётчику, реализуется на этапе линковки
namespace platform {

uint32_t now_ms();

} // namespace platform
