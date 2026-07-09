#include <doctest/doctest.h>

#include "core/scheduler.hpp"
#include "modules/archive/archive.hpp"
#include "modules/geofence_engine/geofence_engine.hpp"
#include "modules/uplink/uplink.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace {

class RecordingTransport : public modules::uplink::ILinkTransport {
public:
    bool send(const msg::Record* records, std::size_t count) override {
        for (std::size_t i = 0; i < count; ++i) {
            received.push_back(records[i]);
        }
        return true;
    }
    std::vector<msg::Record> received;
};

msg::GpsFix fix_at(int32_t lat, int32_t lon, uint32_t ts) {
    msg::GpsFix fix{};
    fix.lat_e7 = lat;
    fix.lon_e7 = lon;
    fix.timestamp_ms = ts;
    return fix;
}

} // namespace

TEST_CASE("Сквозной сценарий: координата -> событие геозоны -> запись архива -> отправка") {
    msg::GpsTopic gps_topic;
    msg::ZoneEventTopic event_topic;

    msg::GpsTopic::Subscription geo_sub;
    REQUIRE(gps_topic.subscribe(geo_sub));
    modules::geofence_engine::GeofenceTask geofence(std::move(geo_sub), event_topic);
    REQUIRE(geofence.add_zone({0, 0, 1000, 5, 300}));

    msg::ZoneEventTopic::Subscription arch_sub;
    REQUIRE(event_topic.subscribe(arch_sub));
    modules::archive::ArchiveTask archive(std::move(arch_sub));

    RecordingTransport transport;
    modules::uplink::UplinkTask uplink(archive, transport, 500);

    // Порядок регистрации повторяет поток данных: событие проходит всю цепочку за один круг планировщика
    core::Scheduler<10> scheduler;
    REQUIRE(scheduler.add_task(geofence));
    REQUIRE(scheduler.add_task(archive));
    REQUIRE(scheduler.add_task(uplink));

    // «Прерывание» GPS публикует фиксы поверх тиков планировщика каждые 100 мс
    for (uint32_t t = 0; t <= 3000; t += 100) {
        if (t == 0) {
            gps_topic.publish(fix_at(5000, 0, t)); // старт снаружи
        }
        if (t == 1000) {
            gps_topic.publish(fix_at(0, 0, t)); // вход в зону
        }
        if (t == 2000) {
            gps_topic.publish(fix_at(4000, 0, t)); // выход из зоны
        }
        scheduler.run_once(t);
    }

    // До «сервера» дошли ровно две записи в порядке возникновения
    REQUIRE(transport.received.size() == 2);

    CHECK(transport.received[0].seq == 1);
    CHECK(transport.received[0].kind == msg::RecordKind::ZoneEvent);
    CHECK(transport.received[0].event.zone_id == 5);
    CHECK(transport.received[0].event.type == msg::ZoneEventType::Entered);
    CHECK(transport.received[0].event.timestamp_ms == 1000);

    CHECK(transport.received[1].seq == 2);
    CHECK(transport.received[1].event.type == msg::ZoneEventType::Exited);
    CHECK(transport.received[1].event.timestamp_ms == 2000);

    // Ничего не потеряно ни на одном звене цепочки
    CHECK(geofence.dropped_events() == 0);
    CHECK(archive.evicted_count() == 0);
    CHECK(uplink.pending_records() == 0);
    CHECK(uplink.sent_packets() == 2);
}
