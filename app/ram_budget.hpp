#pragma once

#include "core/scheduler.hpp"
#include "modules/archive/archive.hpp"
#include "modules/diagnostics/diagnostics.hpp"
#include "modules/geofence_engine/geofence_engine.hpp"
#include "modules/gps/gps.hpp"
#include "modules/sensors/sensors.hpp"
#include "modules/uplink/uplink.hpp"
#include "msg/topics.hpp"

#include <cstddef>
#include <type_traits>

// Единая точка учёта ОЗУ. Все ёмкости - константы компиляции, поэтому суммарный след считается статически. При выходе за бюджет сборка падает здесь.
namespace budget {

inline constexpr std::size_t kTotalRamBytes = 128u * 1024u;
inline constexpr std::size_t kStackReserveBytes = 16u * 1024u;   // стек main-цикла и ISR
inline constexpr std::size_t kRuntimeReserveBytes = 16u * 1024u; // статика libc/платформы
inline constexpr std::size_t kModuleBudgetBytes =
    kTotalRamBytes - kStackReserveBytes - kRuntimeReserveBytes;

// Планировщик системы: ровно 10 модулей
using SystemScheduler = core::Scheduler<10>;

inline constexpr std::size_t kSystemFootprintBytes =
    sizeof(msg::GpsTopic) + sizeof(msg::ZoneEventTopic) + sizeof(SystemScheduler) +
    sizeof(modules::gps::GpsSimTask) +
    sizeof(modules::geofence_engine::GeofenceTask) +
    sizeof(modules::archive::ArchiveTask) +
    sizeof(modules::uplink::UplinkTask) +
    sizeof(modules::sensors::SensorPollTask) +
    sizeof(modules::diagnostics::WatchdogTask);

static_assert(kSystemFootprintBytes <= kModuleBudgetBytes,
              "Static footprint of system objects exceeds the RAM budget: "
              "shrink queue/zone/probe capacities in msg/topics.hpp and modules");

// Сообщения копируются в кольца побайтово,т.к. нетривиальный тип сломает каналы
static_assert(std::is_trivially_copyable_v<msg::GpsFix> &&
                  std::is_trivially_copyable_v<msg::ZoneEvent> &&
                  std::is_trivially_copyable_v<msg::Record>,
              "Channel messages must stay trivially copyable");

} // namespace budget
