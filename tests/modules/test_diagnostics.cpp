#include <doctest/doctest.h>

#include "modules/diagnostics/diagnostics.hpp"

using modules::diagnostics::HealthProbe;
using modules::diagnostics::WatchdogTask;

namespace {

struct FakeModule {
    uint32_t counter = 0;
};

uint32_t read_counter(const void* module) {
    return static_cast<const FakeModule*>(module)->counter;
}

struct StallLog {
    const char* name = nullptr;
    uint32_t at_ms = 0;
    int calls = 0;
};

void on_stall(const char* name, uint32_t now_ms, void* user) {
    auto* log = static_cast<StallLog*>(user);
    log->name = name;
    log->at_ms = now_ms;
    ++log->calls;
}

} // namespace

TEST_CASE("WatchdogTask: растущий счётчик остаётся здоровым") {
    FakeModule mod;
    StallLog log;
    WatchdogTask watchdog(on_stall, &log);
    REQUIRE(watchdog.add_probe({"mod", 1000, read_counter, &mod}));

    for (uint32_t t = 0; t <= 5000; t += 500) {
        ++mod.counter;
        watchdog.tick(t);
    }

    CHECK(watchdog.system_healthy());
    CHECK(watchdog.stall_events() == 0);
    CHECK(log.calls == 0);
}

TEST_CASE("WatchdogTask: замерший счётчик объявляется stalled строго после дедлайна") {
    FakeModule mod;
    StallLog log;
    WatchdogTask watchdog(on_stall, &log);
    REQUIRE(watchdog.add_probe({"mod", 1000, read_counter, &mod}));

    watchdog.tick(0); // инициализация базовой точки
    watchdog.tick(900);
    CHECK(watchdog.system_healthy());

    watchdog.tick(1000); // ровно дедлайн -- ещё не зависание
    CHECK(watchdog.system_healthy());

    watchdog.tick(1100); // дедлайн превышен
    CHECK_FALSE(watchdog.system_healthy());
    CHECK(watchdog.stall_events() == 1);
    CHECK(log.calls == 1);
    CHECK(log.at_ms == 1100);

    watchdog.tick(2000); // продолжение застоя не спамит колбэком
    CHECK(log.calls == 1);
}

TEST_CASE("WatchdogTask: возобновление прогресса снимает диагноз") {
    FakeModule mod;
    StallLog log;
    WatchdogTask watchdog(on_stall, &log);
    REQUIRE(watchdog.add_probe({"mod", 1000, read_counter, &mod}));

    watchdog.tick(0);
    watchdog.tick(1100); // зависание
    CHECK_FALSE(watchdog.system_healthy());

    ++mod.counter; // модуль ожил
    watchdog.tick(1200);
    CHECK(watchdog.system_healthy());
    CHECK(watchdog.stall_events() == 1);

    watchdog.tick(2300); // снова замер: отсчёт от момента оживления
    CHECK_FALSE(watchdog.system_healthy());
    CHECK(watchdog.stall_events() == 2);
    CHECK(log.calls == 2);
}

TEST_CASE("WatchdogTask: отказ при переполнении таблицы проб") {
    FakeModule mod;
    WatchdogTask watchdog;
    for (std::size_t i = 0; i < WatchdogTask::kMaxProbes; ++i) {
        REQUIRE(watchdog.add_probe({"mod", 1000, read_counter, &mod}));
    }
    CHECK_FALSE(watchdog.add_probe({"extra", 1000, read_counter, &mod}));
    CHECK(watchdog.probe_count() == WatchdogTask::kMaxProbes);
}
