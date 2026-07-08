#include <doctest/doctest.h>

#include "core/scheduler.hpp"
#include "modules/sensors/sensors.hpp"
#include "platform/clock_hal.hpp"

namespace {

// Считает обращения и возвращает счётчик как значение
class CountingSource : public modules::sensors::ISampleSource {
public:
    uint16_t read() override { return ++reads_; }
    uint16_t reads_ = 0;
};

} // namespace

TEST_CASE("SensorPollTask: опрос источника с заданным периодом") {
    CountingSource source;
    modules::sensors::SensorPollTask task(source, 10);

    core::Scheduler<4> scheduler;
    REQUIRE(scheduler.add_task(task));

    // Модуль отсчитывает время сам через platform::now_ms(), поэтому тест вынужден гонять планировщик по реальным часам
    const uint32_t start = platform::now_ms();
    while (platform::now_ms() - start < 55) {
        scheduler.run_once();
    }

    /* За ~55 мс при периоде 10 мс ожидаем ~6 выборок, но точное число зависит от загрузки машины и
     * разрешения часов, поэтому проверяем вилкой, а не равенством */
    CHECK(task.sample_count() >= 4);
    CHECK(task.sample_count() <= 7);
    CHECK(task.last_sample() == task.sample_count());
}
