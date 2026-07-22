#include <doctest/doctest.h>

#include "telemetry_publisher.hpp"

#include <string>
#include <utility>
#include <vector>

namespace {

class CollectSink : public demo::ITextSink {
public:
    bool send_line(const char* line) override {
        if (!online) {
            return false;
        }
        lines.emplace_back(line);
        return true;
    }
    bool online = true;
    std::vector<std::string> lines;
};

} // namespace

TEST_CASE("TelemetryPublisher: фиксы и события уходят строками NDJSON") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    msg::ZoneEventTopic::Subscription ev_sub;
    REQUIRE(gps.subscribe(gps_sub));
    REQUIRE(events.subscribe(ev_sub));

    CollectSink sink;
    demo::TelemetryPublisher publisher(std::move(gps_sub), std::move(ev_sub), sink);

    msg::GpsFix fix{};
    fix.lat_e7 = 557600000;
    fix.lon_e7 = 376000000;
    fix.speed_kmh_x10 = 5;
    fix.timestamp_ms = 1000;
    gps.publish(fix);

    msg::ZoneEvent event{};
    event.zone_id = 1;
    event.type = msg::ZoneEventType::Entered;
    event.lat_e7 = 10;
    event.lon_e7 = -20;
    event.timestamp_ms = 2000;
    events.publish(event);

    publisher.tick(0);

    REQUIRE(sink.lines.size() == 2);
    CHECK(sink.lines[0] ==
          "{\"type\":\"fix\",\"lat_e7\":557600000,\"lon_e7\":376000000,"
          "\"speed_x10\":5,\"t\":1000}");
    CHECK(sink.lines[1] ==
          "{\"type\":\"event\",\"zone\":1,\"kind\":\"enter\","
          "\"lat_e7\":10,\"lon_e7\":-20,\"t\":2000}");
    CHECK(publisher.dropped_lines() == 0);
}

TEST_CASE("TelemetryPublisher: недоступный канал считает потери и не блокирует") {
    msg::GpsTopic gps;
    msg::ZoneEventTopic events;
    msg::GpsTopic::Subscription gps_sub;
    msg::ZoneEventTopic::Subscription ev_sub;
    REQUIRE(gps.subscribe(gps_sub));
    REQUIRE(events.subscribe(ev_sub));

    CollectSink sink;
    sink.online = false;
    demo::TelemetryPublisher publisher(std::move(gps_sub), std::move(ev_sub), sink);

    msg::GpsFix fix{};
    gps.publish(fix);
    gps.publish(fix);
    publisher.tick(0);
    CHECK(publisher.dropped_lines() == 2);
    CHECK(sink.lines.empty());

    // Дашборд подключился: дальше идут только свежие данные, без буферизации старых
    sink.online = true;
    gps.publish(fix);
    publisher.tick(0);
    CHECK(sink.lines.size() == 1);
    CHECK(publisher.dropped_lines() == 2);
}
