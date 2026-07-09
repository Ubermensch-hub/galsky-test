#pragma once

#include "core/task.hpp"
#include "msg/topics.hpp"

#include <cstddef>
#include <cstdint>

// Модуль 1 - GPS/GNSS (Приём координат и скорости)
namespace modules::gps {

// Точка маршрута симулятора
struct Waypoint {
    int32_t lat_e7;
    int32_t lon_e7;
};

// Симулятор приёмника: публикует точки маршрута в топик по сетке периодов,
// на последней точке «стоит» (продолжает публиковать её же). На железе вместо
// него -- разбор NMEA из UART (ISR-путь); контракт для потребителей тот же:
// фиксы появляются в GpsTopic
class GpsSimTask : public core::ITask {
public:
    GpsSimTask(msg::GpsTopic& topic, const Waypoint* path, std::size_t count,
               uint32_t period_ms)
        : topic_(&topic), path_(path), count_(count), period_ms_(period_ms) {}

    void tick(uint32_t now_ms) override {
        if (!started_) {
            started_ = true;
            next_fix_ms_ = now_ms;
        }
        // Сравнение через знаковую разность корректно при переполнении uint32_t
        if (static_cast<int32_t>(now_ms - next_fix_ms_) < 0) {
            return;
        }
        next_fix_ms_ += period_ms_;
        if (count_ == 0) {
            return;
        }

        msg::GpsFix fix{};
        fix.lat_e7 = path_[index_].lat_e7;
        fix.lon_e7 = path_[index_].lon_e7;
        fix.timestamp_ms = now_ms;
        dropped_ += static_cast<uint32_t>(topic_->publish(fix));
        ++published_;
        if (index_ + 1 < count_) {
            ++index_;
        }
    }

    uint32_t published_fixes() const { return published_; }

    // Потери на переполненных кольцах подписчиков (для диагностики)
    uint32_t dropped_fixes() const { return dropped_; }

private:
    msg::GpsTopic* topic_;
    const Waypoint* path_;
    std::size_t count_;
    uint32_t period_ms_;
    std::size_t index_{0};
    uint32_t next_fix_ms_{0};
    bool started_{false};
    uint32_t published_{0};
    uint32_t dropped_{0};
};

} // namespace modules::gps
