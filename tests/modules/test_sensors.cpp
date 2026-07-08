#include <doctest/doctest.h>

#include "core/scheduler.hpp"
#include "modules/sensors/sensors.hpp"

namespace {

// Считает обращения и возвращает счётчик как значение
class CountingSource : public modules::sensors::ISampleSource {
public:
    uint16_t read() override { return ++reads_; }
    uint16_t reads_ = 0;
};

} // namespace

TEST_CASE("SensorPollTask: выборки строго по сетке периодов") {
    CountingSource source;
    modules::sensors::SensorPollTask task(source, 10);

    core::Scheduler<4> scheduler;
    REQUIRE(scheduler.add_task(task));

    scheduler.run_once(1000);
    CHECK(task.sample_count() == 1);

    scheduler.run_once(1005);
    CHECK(task.sample_count() == 1);

    scheduler.run_once(1010);
    CHECK(task.sample_count() == 2);

    scheduler.run_once(1019);
    CHECK(task.sample_count() == 2);

    scheduler.run_once(1025);
    CHECK(task.sample_count() == 3);

    scheduler.run_once(1030);
    CHECK(task.sample_count() == 4);

    CHECK(task.last_sample() == task.sample_count());
}

TEST_CASE("SensorPollTask: период переживает переполнение uint32_t миллисекунд") {
    CountingSource source;
    modules::sensors::SensorPollTask task(source, 10);

    core::Scheduler<4> scheduler;
    REQUIRE(scheduler.add_task(task));

    // ~49.7 суток аптайма: счётчик миллисекунд на пороге переворота
    scheduler.run_once(0xFFFFFFFFu - 4);
    CHECK(task.sample_count() == 1);

    scheduler.run_once(0xFFFFFFFFu);
    CHECK(task.sample_count() == 1);

    scheduler.run_once(5u); // срок наступил после переворота счётчика
    CHECK(task.sample_count() == 2);

    scheduler.run_once(15u);
    CHECK(task.sample_count() == 3);
}
