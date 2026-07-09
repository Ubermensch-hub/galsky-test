#pragma once

#include <cstdint>

// Словарь сообщений между модулями фиксированного размера,
// копируются в кольца каналов целиком, указателей не содержат
namespace msg {

// Фикс GPS: координаты целочисленные в 1e-7 градуса, скорость в 0.1 км/ч, время
struct GpsFix {
    int32_t lat_e7;
    int32_t lon_e7;
    uint16_t speed_kmh_x10;
    uint32_t timestamp_ms;
};

enum class ZoneEventType : uint8_t { Entered = 0, Exited = 1 };

// Событие геозоны: несёт координату и время фикса, породившего переход
struct ZoneEvent {
    uint8_t zone_id;
    ZoneEventType type;
    int32_t lat_e7;
    int32_t lon_e7;
    uint32_t timestamp_ms;
};

} // namespace msg
