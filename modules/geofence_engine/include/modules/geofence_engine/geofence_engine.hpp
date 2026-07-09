#pragma once

#include "core/task.hpp"
#include "msg/topics.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

// Модуль 4 - Движок геозон и событий (Геозоны, превышение скорости, пороговые значения)
namespace modules::geofence_engine {

// Круговая геозона. Планарное приближение: радиус в единицах 1e-7 градуса
struct Zone {
    int32_t center_lat_e7;
    int32_t center_lon_e7;
    uint32_t radius_e7;
    uint8_t id;
};

// Кооперативная задача: вычитывает фиксы GPS, отслеживает переходы границ зон и публикует события.
class GeofenceTask : public core::ITask {
public:
    static constexpr std::size_t kMaxZones = 8;

    GeofenceTask(msg::GpsTopic::Subscription&& gps, msg::ZoneEventTopic& events)
        : gps_(std::move(gps)), events_(&events) {}

    // false - таблица зон заполнена
    bool add_zone(const Zone& zone) {
        if (zone_count_ == kMaxZones) {
            return false;
        }
        slots_[zone_count_++] = {zone, State::Unknown};
        return true;
    }

    void tick(uint32_t) override {
        msg::GpsFix fix;
        while (gps_.try_pop(fix)) { // цикл ограничен ёмкостью кольца
            for (std::size_t i = 0; i < zone_count_; ++i) {
                process(slots_[i], fix);
            }
        }
    }

    std::size_t zone_count() const { return zone_count_; }

    // События, потерянные из-за переполнения колец подписчиков (для диагностики)
    uint32_t dropped_events() const { return dropped_events_; }

private:
    enum class State : uint8_t { Unknown, Inside, Outside };

    struct Slot {
        Zone zone;
        State state;
    };

    static bool contains(const Zone& zone, const msg::GpsFix& fix) {
        const int64_t dlat = static_cast<int64_t>(fix.lat_e7) - zone.center_lat_e7;
        const int64_t dlon = static_cast<int64_t>(fix.lon_e7) - zone.center_lon_e7;
        const int64_t radius = zone.radius_e7;
        return dlat * dlat + dlon * dlon <= radius * radius;
    }

    void process(Slot& slot, const msg::GpsFix& fix) {
        const bool in = contains(slot.zone, fix);
        if (slot.state == State::Unknown) {
            slot.state = in ? State::Inside : State::Outside;
            return;
        }
        if (in && slot.state == State::Outside) {
            slot.state = State::Inside;
            emit(slot.zone, fix, msg::ZoneEventType::Entered);
        } else if (!in && slot.state == State::Inside) {
            slot.state = State::Outside;
            emit(slot.zone, fix, msg::ZoneEventType::Exited);
        }
    }

    void emit(const Zone& zone, const msg::GpsFix& fix, msg::ZoneEventType type) {
        msg::ZoneEvent event{};
        event.zone_id = zone.id;
        event.type = type;
        event.lat_e7 = fix.lat_e7;
        event.lon_e7 = fix.lon_e7;
        event.timestamp_ms = fix.timestamp_ms;
        dropped_events_ += static_cast<uint32_t>(events_->publish(event));
    }

    msg::GpsTopic::Subscription gps_;
    msg::ZoneEventTopic* events_;
    std::array<Slot, kMaxZones> slots_{};
    std::size_t zone_count_{0};
    uint32_t dropped_events_{0};
};

} // namespace modules::geofence_engine
