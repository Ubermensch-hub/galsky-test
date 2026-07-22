#pragma once

#include <cstdint>

// Доступ к счётчику, реализуется на этапе линковки
namespace platform {

uint32_t now_ms();

// Пауза главного цикла; на МК здесь idle/WFI, на desktop -- сон потока
void sleep_ms(uint32_t ms);

} // namespace platform
