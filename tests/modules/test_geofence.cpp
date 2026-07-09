#include <doctest/doctest.h>

#include "modules/geofence_engine/geofence_engine.hpp"

#include <utility>

using modules::geofence_engine::GeofenceTask;
using modules::geofence_engine::Zone;

namespace {

msg::GpsFix fix_at(int32_t lat, int32_t lon, uint32_t ts = 0) {
    msg::GpsFix fix{};
    fix.lat_e7 = lat;
    fix.lon_e7 = lon;
    fix.timestamp_ms = ts;
    return fix;
}

} // namespace

TEST_CASE("GeofenceTask: первый фикс инициализирует состояние без события") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    msg::ZoneEventTopic::Subscription ev_sub;
    REQUIRE(gps.subscribe(gps_sub));
    REQUIRE(events.subscribe(ev_sub));

    GeofenceTask task(std::move(gps_sub), events);
    REQUIRE(task.add_zone({0, 0, 1000, 1}));

    gps.publish(fix_at(0, 0)); // старт прямо в центре зоны
    task.tick(0);

    msg::ZoneEvent event;
    CHECK_FALSE(ev_sub.try_pop(event));
}

TEST_CASE("GeofenceTask: пересекание геозоны снаружи внутрь вызывает Entered") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    msg::ZoneEventTopic::Subscription ev_sub;
    REQUIRE(gps.subscribe(gps_sub));
    REQUIRE(events.subscribe(ev_sub));

    GeofenceTask task(std::move(gps_sub), events);
    REQUIRE(task.add_zone({0, 0, 1000, 7}));

    gps.publish(fix_at(5000, 0, 100));
    gps.publish(fix_at(500, 0, 200));
    task.tick(0);

    msg::ZoneEvent event;
    REQUIRE(ev_sub.try_pop(event));
    CHECK(event.zone_id == 7);
    CHECK(event.type == msg::ZoneEventType::Entered);
    CHECK(event.lat_e7 == 500);
    CHECK(event.timestamp_ms == 200); // время фикса, породившего переход
    CHECK_FALSE(ev_sub.try_pop(event));
}

TEST_CASE("GeofenceTask: переход изнутри наружу порождает Exited") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    msg::ZoneEventTopic::Subscription ev_sub;
    REQUIRE(gps.subscribe(gps_sub));
    REQUIRE(events.subscribe(ev_sub));

    GeofenceTask task(std::move(gps_sub), events);
    REQUIRE(task.add_zone({0, 0, 1000, 3}));

    gps.publish(fix_at(0, 0));       // инициализация: внутри
    gps.publish(fix_at(0, 4000));    // выход
    task.tick(0);

    msg::ZoneEvent event;
    REQUIRE(ev_sub.try_pop(event));
    CHECK(event.zone_id == 3);
    CHECK(event.type == msg::ZoneEventType::Exited);
    CHECK_FALSE(ev_sub.try_pop(event));
}

TEST_CASE("GeofenceTask: движение без пересечения границы не порождает событий") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    msg::ZoneEventTopic::Subscription ev_sub;
    REQUIRE(gps.subscribe(gps_sub));
    REQUIRE(events.subscribe(ev_sub));

    GeofenceTask task(std::move(gps_sub), events);
    REQUIRE(task.add_zone({0, 0, 1000, 1}));

    gps.publish(fix_at(5000, 0));
    gps.publish(fix_at(4000, 0));
    gps.publish(fix_at(0, 300));
    gps.publish(fix_at(0, 600));
    task.tick(0);

    msg::ZoneEvent event;
    REQUIRE(ev_sub.try_pop(event));
    CHECK(event.type == msg::ZoneEventType::Entered);
    CHECK_FALSE(ev_sub.try_pop(event));
}

TEST_CASE("GeofenceTask: зоны отслеживаются независимо") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    msg::ZoneEventTopic::Subscription ev_sub;
    REQUIRE(gps.subscribe(gps_sub));
    REQUIRE(events.subscribe(ev_sub));

    GeofenceTask task(std::move(gps_sub), events);
    REQUIRE(task.add_zone({0, 0, 1000, 1}));
    REQUIRE(task.add_zone({1500, 0, 1000, 2}));

    gps.publish(fix_at(5000, 0));
    gps.publish(fix_at(750, 0));
    gps.publish(fix_at(1800, 0));
    task.tick(0);

    msg::ZoneEvent event;
    REQUIRE(ev_sub.try_pop(event));
    CHECK(event.zone_id == 1);
    CHECK(event.type == msg::ZoneEventType::Entered);

    REQUIRE(ev_sub.try_pop(event));
    CHECK(event.zone_id == 2);
    CHECK(event.type == msg::ZoneEventType::Entered);

    REQUIRE(ev_sub.try_pop(event));
    CHECK(event.zone_id == 1);
    CHECK(event.type == msg::ZoneEventType::Exited);

    CHECK_FALSE(ev_sub.try_pop(event));
}

TEST_CASE("GeofenceTask: потерянные события считаются") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    msg::ZoneEventTopic::Subscription ev_sub; // подписан, но не читает
    REQUIRE(gps.subscribe(gps_sub));
    REQUIRE(events.subscribe(ev_sub));

    GeofenceTask task(std::move(gps_sub), events);
    REQUIRE(task.add_zone({0, 0, 1000, 1}));

    gps.publish(fix_at(5000, 0)); // инициализация: снаружи
    task.tick(0);

    // 8 переходов при полезной ёмкости кольца 7: последнее событие теряется
    for (int i = 0; i < 4; ++i) {
        gps.publish(fix_at(0, 0));
        task.tick(0);
        gps.publish(fix_at(5000, 0));
        task.tick(0);
    }

    CHECK(task.dropped_events() == 1);
}

TEST_CASE("GeofenceTask: отказ при переполнении таблицы зон") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    REQUIRE(gps.subscribe(gps_sub));

    GeofenceTask task(std::move(gps_sub), events);
    for (uint8_t i = 0; i < GeofenceTask::kMaxZones; ++i) {
        REQUIRE(task.add_zone({0, 0, 1000, i}));
    }
    CHECK_FALSE(task.add_zone({0, 0, 1000, 99}));
    CHECK(task.zone_count() == GeofenceTask::kMaxZones);
}
 // Джиттер-тест
TEST_CASE("GeofenceTask: дрожание фиксов у границы зоны не порождает спам событий") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    msg::ZoneEventTopic::Subscription ev_sub;
    REQUIRE(gps.subscribe(gps_sub));
    REQUIRE(events.subscribe(ev_sub));

    GeofenceTask task(std::move(gps_sub), events);
    REQUIRE(task.add_zone({0, 0, 1000, 1, 300})); // гистерезис шире амплитуды дрожания

    gps.publish(fix_at(5000, 0)); // инициализация: снаружи
    task.tick(0);


    const int32_t jitter[] = {995, 1120, 980, 1250, 1010, 1180, 990, 1290, 1005};
    for (int32_t lat : jitter) {
        gps.publish(fix_at(lat, 0));
        task.tick(0);
    }

    // Ожидание: одно Entered при первом пересечении и никакого спама
    int entered = 0;
    int exited = 0;
    msg::ZoneEvent event;
    while (ev_sub.try_pop(event)) {
        if (event.type == msg::ZoneEventType::Entered) {
            ++entered;
        } else {
            ++exited;
        }
    }
    CHECK(entered == 1);
    CHECK(exited == 0);

    // Настоящий выход
    gps.publish(fix_at(1400, 0));
    task.tick(0);
    REQUIRE(ev_sub.try_pop(event));
    CHECK(event.type == msg::ZoneEventType::Exited);
    CHECK_FALSE(ev_sub.try_pop(event));
}
