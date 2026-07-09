#include "core/scheduler.hpp"
#include "modules/archive/archive.hpp"
#include "modules/geofence_engine/geofence_engine.hpp"
#include "modules/gps/gps.hpp"
#include "modules/sensors/sensors.hpp"
#include "modules/uplink/uplink.hpp"
#include "msg/topics.hpp"
#include "platform/clock_hal.hpp"

#include <cstdint>
#include <cstdio>

// Демонстрационный запуск: 10 задач в одном планировщике, сквозной сценарий
// координата -> событие геозоны -> запись архива -> отправка на сервер.
// Время симулированное, на железе тот же цикл работает от таймера
namespace {

// Синтетический датчик вместо АЦП
class SyntheticSensor : public modules::sensors::ISampleSource {
public:
    uint16_t read() override {
        value_ = static_cast<uint16_t>(2000 + (value_ * 13 + 37) % 700);
        return value_;
    }

private:
    uint16_t value_ = 0;
};

// Транспорт-заглушка: печатает пакет в консоль и подтверждает доставку
class ConsoleTransport : public modules::uplink::ILinkTransport {
public:
    bool send(const msg::Record* records, std::size_t count) override {
        std::printf("           uplink: packet sent, records: %u\n",
                    static_cast<unsigned>(count));
        for (std::size_t i = 0; i < count; ++i) {
            const msg::Record& r = records[i];
            std::printf("             seq=%u %s %u (t=%u ms)\n", r.seq,
                        r.event.type == msg::ZoneEventType::Entered ? "entered zone"
                                                                    : "exited zone",
                        r.event.zone_id, r.event.timestamp_ms);
        }
        return true;
    }
};

// Модуль-заглушка: подтверждает жизнь в собственном ритме
class HeartbeatTask : public core::ITask {
public:
    HeartbeatTask(const char* name, uint32_t period_ms)
        : name_(name), period_ms_(period_ms) {}

    void tick(uint32_t now_ms) override {
        if (!started_) {
            started_ = true;
            next_ms_ = now_ms;
        }
        if (static_cast<int32_t>(now_ms - next_ms_) < 0) {
            return;
        }
        next_ms_ += period_ms_;
        std::printf("[%6u ms] %s: stub alive\n", now_ms, name_);
    }

private:
    const char* name_;
    uint32_t period_ms_;
    uint32_t next_ms_{0};
    bool started_{false};
};

// Интерпретатор-заглушка: второй подписчик GPS, пока только считает фиксы
class ScriptStubTask : public core::ITask {
public:
    ScriptStubTask(msg::GpsTopic::Subscription&& gps, uint32_t period_ms)
        : gps_(static_cast<msg::GpsTopic::Subscription&&>(gps)), period_ms_(period_ms) {}

    void tick(uint32_t now_ms) override {
        msg::GpsFix fix;
        while (gps_.try_pop(fix)) {
            ++fixes_;
        }
        if (!started_) {
            started_ = true;
            next_ms_ = now_ms;
        }
        if (static_cast<int32_t>(now_ms - next_ms_) < 0) {
            return;
        }
        next_ms_ += period_ms_;
        std::printf("[%6u ms] script: fixes available to scripts: %u\n", now_ms, fixes_);
    }

private:
    msg::GpsTopic::Subscription gps_;
    uint32_t period_ms_;
    uint32_t fixes_{0};
    uint32_t next_ms_{0};
    bool started_{false};
};

// Диагностика-заглушка: печатает события геозон и сводку счётчиков
class DiagStubTask : public core::ITask {
public:
    DiagStubTask(msg::ZoneEventTopic::Subscription&& events,
                 const modules::archive::ArchiveTask& archive,
                 const modules::uplink::UplinkTask& uplink,
                 const modules::sensors::SensorPollTask& sensors, uint32_t period_ms)
        : events_(static_cast<msg::ZoneEventTopic::Subscription&&>(events)),
          archive_(&archive), uplink_(&uplink), sensors_(&sensors),
          period_ms_(period_ms) {}

    void tick(uint32_t now_ms) override {
        msg::ZoneEvent event;
        while (events_.try_pop(event)) {
            std::printf("[%6u ms] EVENT: %s %u (lat=%d)\n", now_ms,
                        event.type == msg::ZoneEventType::Entered ? "entered zone"
                                                                  : "exited zone",
                        event.zone_id, event.lat_e7);
        }
        if (!started_) {
            started_ = true;
            next_ms_ = now_ms;
        }
        if (static_cast<int32_t>(now_ms - next_ms_) < 0) {
            return;
        }
        next_ms_ += period_ms_;
        std::printf("[%6u ms] diag: archive=%u packets=%u records=%u adc=%u\n", now_ms,
                    static_cast<unsigned>(archive_->size()), uplink_->sent_packets(),
                    uplink_->sent_records(), sensors_->last_sample());
    }

private:
    msg::ZoneEventTopic::Subscription events_;
    const modules::archive::ArchiveTask* archive_;
    const modules::uplink::UplinkTask* uplink_;
    const modules::sensors::SensorPollTask* sensors_;
    uint32_t period_ms_;
    uint32_t next_ms_{0};
    bool started_{false};
};

} // namespace

int main() {
    std::printf("=== Tracker: framework demo ===\n");
    std::printf("platform clock at start: %u ms (implementation swapped at link time)\n",
                platform::now_ms());
    std::printf("simulating 30 seconds, scheduler tick 100 ms\n\n");

    // Каналы системы
    msg::GpsTopic gps_topic;
    msg::ZoneEventTopic event_topic;

    // Приближение -> джиттер на границе -> перемещние внутрь -> выход за зону
    static constexpr modules::gps::Waypoint kPath[] = {
        {6000, 0}, {4500, 0}, {3000, 0}, {1500, 0},
        {1100, 0}, {950, 0},  {1250, 0}, {990, 0}, {1150, 0},
        {300, 0},  {100, 0},  {0, 0},    {0, 0},
        {800, 0},  {1600, 0}, {3000, 0}, {5000, 0},
    };
    modules::gps::GpsSimTask gps(gps_topic, kPath, sizeof(kPath) / sizeof(kPath[0]), 1000);

    // Геозона «база»: параметры пока константами (config_store -- заглушка)
    msg::GpsTopic::Subscription geo_sub;
    if (!gps_topic.subscribe(geo_sub)) {
        return 1;
    }
    modules::geofence_engine::GeofenceTask geofence(
        static_cast<msg::GpsTopic::Subscription&&>(geo_sub), event_topic);
    if (!geofence.add_zone({0, 0, 1000, 1, 300})) {
        return 1;
    }

    msg::ZoneEventTopic::Subscription arch_sub;
    if (!event_topic.subscribe(arch_sub)) {
        return 1;
    }
    modules::archive::ArchiveTask archive(
        static_cast<msg::ZoneEventTopic::Subscription&&>(arch_sub));

    ConsoleTransport transport;
    modules::uplink::UplinkTask uplink(archive, transport, 2000);

    SyntheticSensor adc;
    modules::sensors::SensorPollTask sensors(adc, 500);

    msg::GpsTopic::Subscription script_sub;
    if (!gps_topic.subscribe(script_sub)) {
        return 1;
    }
    ScriptStubTask script(static_cast<msg::GpsTopic::Subscription&&>(script_sub), 5000);

    msg::ZoneEventTopic::Subscription diag_sub;
    if (!event_topic.subscribe(diag_sub)) {
        return 1;
    }
    DiagStubTask diag(static_cast<msg::ZoneEventTopic::Subscription&&>(diag_sub),
                      archive, uplink, sensors, 5000);

    HeartbeatTask can_bus("can_bus", 4000);
    HeartbeatTask power_mgmt("power_mgmt", 6000);
    HeartbeatTask config_store("config_store", 8000);

    // Порядок регистрации повторяет поток данных: событие проходит
    // цепочку gps -> геозоны -> архив -> uplink за один круг
    core::Scheduler<10> scheduler;
    core::ITask* tasks[] = {&gps,    &can_bus, &sensors,     &script, &geofence,
                            &archive, &uplink,  &power_mgmt, &config_store, &diag};
    for (core::ITask* task : tasks) {
        if (!scheduler.add_task(*task)) {
            std::printf("error: task table full\n");
            return 1;
        }
    }
    std::printf("tasks in scheduler: %u of %u\n\n",
                static_cast<unsigned>(scheduler.task_count()),
                static_cast<unsigned>(scheduler.capacity()));

    for (uint32_t t = 0; t <= 30000; t += 100) {
        scheduler.run_once(t);
    }

    std::printf("\n=== Summary ===\n");
    std::printf("fixes published: %u, lost by subscribers: %u\n",
                gps.published_fixes(), gps.dropped_fixes());
    std::printf("events lost by geofence: %u\n", geofence.dropped_events());
    std::printf("archive: remaining %u, evicted %u\n",
                static_cast<unsigned>(archive.size()), archive.evicted_count());
    std::printf("uplink: packets %u, records %u, link failures %u, pending %u\n",
                uplink.sent_packets(), uplink.sent_records(), uplink.failed_sends(),
                static_cast<unsigned>(uplink.pending_records()));
    return 0;
}
