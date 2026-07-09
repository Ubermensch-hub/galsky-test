#include <doctest/doctest.h>

#include "core/scheduler.hpp"
#include "modules/gps/gps.hpp"

using modules::gps::GpsSimTask;
using modules::gps::Waypoint;

TEST_CASE("GpsSimTask: публикует маршрут по сетке периодов и стоит на последней точке") {
    msg::GpsTopic topic;
    msg::GpsTopic::Subscription sub;
    REQUIRE(topic.subscribe(sub));

    const Waypoint path[] = {{10, 0}, {20, 0}, {30, 0}};
    GpsSimTask sim(topic, path, 3, 1000);

    core::Scheduler<2> scheduler;
    REQUIRE(scheduler.add_task(sim));

    scheduler.run_once(0);    // первая точка сразу
    scheduler.run_once(500);  // до срока
    scheduler.run_once(1000); // вторая
    scheduler.run_once(2000); // третья
    scheduler.run_once(3000); // маршрут кончился -- «стоянка» на последней

    msg::GpsFix fix;
    const int32_t expected_lat[] = {10, 20, 30, 30};
    const uint32_t expected_ts[] = {0, 1000, 2000, 3000};
    for (int i = 0; i < 4; ++i) {
        REQUIRE(sub.try_pop(fix));
        CHECK(fix.lat_e7 == expected_lat[i]);
        CHECK(fix.timestamp_ms == expected_ts[i]);
    }
    CHECK_FALSE(sub.try_pop(fix));

    CHECK(sim.published_fixes() == 4);
    CHECK(sim.dropped_fixes() == 0);
}
