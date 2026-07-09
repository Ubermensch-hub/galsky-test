#pragma once

#include "core/topic.hpp"
#include "msg/messages.hpp"

// Конфигурация каналов системы. Все ёмкости зафиксированы в этом файле,
// поэтому память каналов детерминирована и считается на этапе компиляции
namespace msg {

// Фиксы GPS: подписчики - геозоны, интерпретатор скриптов, архив
using GpsTopic = core::Topic<GpsFix, 4, 8>;

// События геозон: подписчики - архив, передача на сервер
using ZoneEventTopic = core::Topic<ZoneEvent, 4, 8>;

} // namespace msg
