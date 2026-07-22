#include "ram_budget.hpp"
#include "telemetry_publisher.hpp"

#include "core/scheduler.hpp"
#include "modules/archive/archive.hpp"
#include "modules/diagnostics/diagnostics.hpp"
#include "modules/geofence_engine/geofence_engine.hpp"
#include "modules/gps/gps.hpp"
#include "modules/sensors/sensors.hpp"
#include "modules/uplink/uplink.hpp"
#include "msg/topics.hpp"
#include "platform/clock_hal.hpp"
#include "platform/net_hal.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

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

// CAN-заглушка с имитацией отказа шины: на 15-й секунде счётчик кадров
// замирает -- это должен поймать watchdog
class FaultyCanTask : public core::ITask {
public:
    void tick(uint32_t now_ms) override {
        if (!started_) {
            started_ = true;
            next_ms_ = now_ms;
            fail_at_ms_ = now_ms + 15000; // отказ через 15 с от старта, не от нуля часов
        }
        if (static_cast<int32_t>(now_ms - fail_at_ms_) < 0) {
            ++frames_;
        }
        if (static_cast<int32_t>(now_ms - next_ms_) < 0) {
            return;
        }
        next_ms_ += 4000;
        std::printf("[%6u ms] can_bus: stub alive, frames=%u\n", now_ms, frames_);
    }

    uint32_t frames() const { return frames_; }

private:
    uint32_t frames_{0};
    uint32_t next_ms_{0};
    uint32_t fail_at_ms_{0};
    bool started_{false};
};

// Мост из publisher в платформенный TCP-канал
struct NetSink : demo::ITextSink {
    bool send_line(const char* line) override { return platform::net_send_line(line); }
};

} // namespace

int main(int argc, char** argv) {
    const bool live = argc > 1 && std::strcmp(argv[1], "--live") == 0;
    std::printf("=== Tracker: framework demo ===\n");
    std::printf("platform clock at start: %u ms (implementation swapped at link time)\n",
                platform::now_ms());
    std::printf("simulating 30 seconds, scheduler tick 100 ms\n\n");

    const struct {
        const char* name;
        std::size_t size;
    } ram_table[] = {
        {"GpsTopic", sizeof(msg::GpsTopic)},
        {"ZoneEventTopic", sizeof(msg::ZoneEventTopic)},
        {"Scheduler<10>", sizeof(budget::SystemScheduler)},
        {"GpsSimTask", sizeof(modules::gps::GpsSimTask)},
        {"GeofenceTask", sizeof(modules::geofence_engine::GeofenceTask)},
        {"ArchiveTask", sizeof(modules::archive::ArchiveTask)},
        {"UplinkTask", sizeof(modules::uplink::UplinkTask)},
        {"SensorPollTask", sizeof(modules::sensors::SensorPollTask)},
        {"WatchdogTask", sizeof(modules::diagnostics::WatchdogTask)},
    };
    std::printf("static RAM footprint of system objects, bytes:\n");
    for (const auto& row : ram_table) {
        std::printf("  %-16s %5u\n", row.name, static_cast<unsigned>(row.size));
    }
    std::printf("  %-16s %5u of %u available (128K minus stack/runtime reserves)\n\n",
                "total", static_cast<unsigned>(budget::kSystemFootprintBytes),
                static_cast<unsigned>(budget::kModuleBudgetBytes));

    // Каналы системы
    msg::GpsTopic gps_topic;
    msg::ZoneEventTopic event_topic;

    // Приближение -> джиттер на границе -> перемещение внутрь -> выход за зону.
    // Координаты реальные (Москва), чтобы дашборд рисовал маршрут на настоящей
    // карте: зона радиусом 45000 ед. == ~0.0045 гр. == ~500 м
    static constexpr int32_t kBaseLat = 557600000; // 55.76 гр. с.ш.
    static constexpr int32_t kBaseLon = 376000000; // 37.60 гр. в.д.
    static constexpr modules::gps::Waypoint kPath[] = {
        {kBaseLat + 270000, kBaseLon}, {kBaseLat + 202500, kBaseLon},
        {kBaseLat + 135000, kBaseLon}, {kBaseLat + 67500, kBaseLon},
        {kBaseLat + 49500, kBaseLon},  {kBaseLat + 42750, kBaseLon},
        {kBaseLat + 56250, kBaseLon},  {kBaseLat + 44550, kBaseLon},
        {kBaseLat + 51750, kBaseLon},  {kBaseLat + 13500, kBaseLon},
        {kBaseLat + 4500, kBaseLon},   {kBaseLat, kBaseLon},
        {kBaseLat, kBaseLon},          {kBaseLat + 36000, kBaseLon},
        {kBaseLat + 72000, kBaseLon},  {kBaseLat + 135000, kBaseLon},
        {kBaseLat + 225000, kBaseLon},
    };
    modules::gps::GpsSimTask gps(gps_topic, kPath, sizeof(kPath) / sizeof(kPath[0]), 1000);

    // Геозона «база»: параметры пока константами (config_store -- заглушка)
    msg::GpsTopic::Subscription geo_sub;
    if (!gps_topic.subscribe(geo_sub)) {
        return 1;
    }
    modules::geofence_engine::GeofenceTask geofence(
        static_cast<msg::GpsTopic::Subscription&&>(geo_sub), event_topic);
    if (!geofence.add_zone({kBaseLat, kBaseLon, 45000, 1, 13500})) {
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

    // События геозон читает и точка входа для немедленной печати в лог
    msg::ZoneEventTopic::Subscription diag_sub;
    if (!event_topic.subscribe(diag_sub)) {
        return 1;
    }

    // Телеметрия для дашборда: ещё два подписчика тех же топиков + TCP-канал.
    // Publisher -- наблюдатель демо, тикается из главного цикла, не из планировщика
    msg::GpsTopic::Subscription pub_gps;
    msg::ZoneEventTopic::Subscription pub_events;
    if (!gps_topic.subscribe(pub_gps) || !event_topic.subscribe(pub_events)) {
        return 1;
    }
    NetSink net_sink;
    demo::TelemetryPublisher publisher(
        static_cast<msg::GpsTopic::Subscription&&>(pub_gps),
        static_cast<msg::ZoneEventTopic::Subscription&&>(pub_events), net_sink);

    FaultyCanTask can_bus;
    HeartbeatTask power_mgmt("power_mgmt", 6000);
    HeartbeatTask config_store("config_store", 8000);

    // Watchdog: пробы прогресса модулей, о зависании сообщает колбэк
    modules::diagnostics::WatchdogTask watchdog(
        [](const char* name, uint32_t now_ms, void*) {
            std::printf("[%6u ms] WATCHDOG: '%s' stalled!\n", now_ms, name);
        },
        nullptr);
    const bool probes_ok =
        watchdog.add_probe({"gps", 3000,
                            [](const void* m) {
                                return static_cast<const modules::gps::GpsSimTask*>(m)
                                    ->published_fixes();
                            },
                            &gps}) &&
        watchdog.add_probe({"sensors", 2000,
                            [](const void* m) {
                                return static_cast<const modules::sensors::SensorPollTask*>(m)
                                    ->sample_count();
                            },
                            &sensors}) &&
        watchdog.add_probe({"can_bus", 3000,
                            [](const void* m) {
                                return static_cast<const FaultyCanTask*>(m)->frames();
                            },
                            &can_bus});
    if (!probes_ok) {
        return 1;
    }

    // Порядок регистрации повторяет поток данных: событие проходит
    // цепочку gps -> геозоны -> архив -> uplink за один круг
    budget::SystemScheduler scheduler;
    const struct {
        core::ITask* task;
        const char* name;
    } tasks[] = {{&gps, "gps"},           {&can_bus, "can_bus"},
                 {&sensors, "sensors"},   {&script, "script"},
                 {&geofence, "geofence"}, {&archive, "archive"},
                 {&uplink, "uplink"},     {&power_mgmt, "power_mgmt"},
                 {&config_store, "config_store"}, {&watchdog, "watchdog"}};
    for (const auto& entry : tasks) {
        if (!scheduler.add_task(*entry.task, entry.name)) {
            std::printf("error: task table full\n");
            return 1;
        }
    }
    std::printf("tasks in scheduler: %u of %u\n\n",
                static_cast<unsigned>(scheduler.task_count()),
                static_cast<unsigned>(scheduler.capacity()));

    // Живой режим для дашборда: реальное время, бесконечный цикл,
    // NDJSON-стрим на tcp://127.0.0.1:45555
    if (live) {
        const bool net_ok = platform::net_listen(45555);
        std::printf(net_ok ? "live mode: telemetry on tcp://127.0.0.1:45555 (NDJSON), "
                             "Ctrl+C to stop\n\n"
                           : "live mode: network unavailable, console only\n\n");
        uint32_t next_diag_ms = platform::now_ms();
        for (;;) {
            const uint32_t now = platform::now_ms();
            platform::net_poll();
            scheduler.run_once(now);
            publisher.tick(now);

            msg::ZoneEvent event;
            while (diag_sub.try_pop(event)) {
                std::printf("[%6u ms] EVENT: %s %u (lat=%d)\n", now,
                            event.type == msg::ZoneEventType::Entered ? "entered zone"
                                                                      : "exited zone",
                            event.zone_id, event.lat_e7);
            }
            if (static_cast<int32_t>(now - next_diag_ms) >= 0) {
                next_diag_ms += 5000;
                std::printf("[%6u ms] diag: archive=%u packets=%u adc=%u watchdog=%s\n",
                            now, static_cast<unsigned>(archive.size()),
                            uplink.sent_packets(), sensors.last_sample(),
                            watchdog.system_healthy() ? "OK" : "STALLED");
            }
            platform::sleep_ms(50);
        }
    }

    for (uint32_t t = 0; t <= 30000; t += 100) {
        scheduler.run_once(t);
        publisher.tick(t);

        msg::ZoneEvent event;
        while (diag_sub.try_pop(event)) {
            std::printf("[%6u ms] EVENT: %s %u (lat=%d)\n", t,
                        event.type == msg::ZoneEventType::Entered ? "entered zone"
                                                                  : "exited zone",
                        event.zone_id, event.lat_e7);
        }
        if (t % 5000 == 0) {
            std::printf("[%6u ms] diag: archive=%u packets=%u adc=%u watchdog=%s\n", t,
                        static_cast<unsigned>(archive.size()), uplink.sent_packets(),
                        sensors.last_sample(),
                        watchdog.system_healthy() ? "OK" : "STALLED");
        }
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
    std::printf("watchdog: stall events %u, system %s\n", watchdog.stall_events(),
                watchdog.system_healthy() ? "healthy" : "DEGRADED");
    std::printf("\ntask            ticks\n");
    for (std::size_t i = 0; i < scheduler.task_count(); ++i) {
        std::printf("  %-14s %u\n", scheduler.task_name(i), scheduler.task_ticks(i));
    }
    return 0;
}
