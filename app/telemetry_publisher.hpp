#pragma once

#include "core/task.hpp"
#include "msg/topics.hpp"

#include <cstdint>
#include <cstdio>
#include <utility>

// Демо-обвязка десктопа: стримит телеметрию строками JSON (NDJSON) во
// внедрённый канал для дашборда. Не входит в 10 модулей прошивки -- на
// устройстве этого наблюдателя нет, поэтому живёт в app/ и в бюджет не входит
namespace demo {

class ITextSink {
public:
    virtual ~ITextSink() = default;
    // false -- получатель недоступен, строка отбрасывается
    virtual bool send_line(const char* line) = 0;
};

class TelemetryPublisher : public core::ITask {
public:
    TelemetryPublisher(msg::GpsTopic::Subscription&& gps,
                       msg::ZoneEventTopic::Subscription&& events, ITextSink& sink)
        : gps_(std::move(gps)), events_(std::move(events)), sink_(&sink) {}

    void tick(uint32_t) override {
        char line[160];
        msg::GpsFix fix;
        while (gps_.try_pop(fix)) {
            std::snprintf(line, sizeof(line),
                          "{\"type\":\"fix\",\"lat_e7\":%ld,\"lon_e7\":%ld,"
                          "\"speed_x10\":%u,\"t\":%lu}",
                          static_cast<long>(fix.lat_e7), static_cast<long>(fix.lon_e7),
                          static_cast<unsigned>(fix.speed_kmh_x10),
                          static_cast<unsigned long>(fix.timestamp_ms));
            if (!sink_->send_line(line)) {
                ++dropped_;
            }
        }
        msg::ZoneEvent event;
        while (events_.try_pop(event)) {
            std::snprintf(line, sizeof(line),
                          "{\"type\":\"event\",\"zone\":%u,\"kind\":\"%s\","
                          "\"lat_e7\":%ld,\"lon_e7\":%ld,\"t\":%lu}",
                          static_cast<unsigned>(event.zone_id),
                          event.type == msg::ZoneEventType::Entered ? "enter" : "exit",
                          static_cast<long>(event.lat_e7),
                          static_cast<long>(event.lon_e7),
                          static_cast<unsigned long>(event.timestamp_ms));
            if (!sink_->send_line(line)) {
                ++dropped_;
            }
        }
    }

    uint32_t dropped_lines() const { return dropped_; }

private:
    msg::GpsTopic::Subscription gps_;
    msg::ZoneEventTopic::Subscription events_;
    ITextSink* sink_;
    uint32_t dropped_{0};
};

} // namespace demo
