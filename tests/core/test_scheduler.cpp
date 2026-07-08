#include <doctest/doctest.h>

#include "core/scheduler.hpp"
#include "core/task.hpp"

#include <vector>

namespace {

// Пишет свой id в общий журнал при каждом tick
class LoggingTask : public core::ITask {
public:
    LoggingTask(int id, std::vector<int>& log) : id_(id), log_(&log) {}
    void tick() override { log_->push_back(id_); }

private:
    int id_;
    std::vector<int>* log_;
};

} // namespace

TEST_CASE("Scheduler: пустой планировщик безопасно выполняет run_once") {
    core::Scheduler<4> scheduler;
    CHECK(scheduler.task_count() == 0);
    scheduler.run_once();
}

TEST_CASE("Scheduler: задачи тикаются в порядке регистрации, по разу за круг") {
    std::vector<int> log;
    LoggingTask a(1, log);
    LoggingTask b(2, log);
    LoggingTask c(3, log);

    core::Scheduler<4> scheduler;
    REQUIRE(scheduler.add_task(a));
    REQUIRE(scheduler.add_task(b));
    REQUIRE(scheduler.add_task(c));
    CHECK(scheduler.task_count() == 3);

    scheduler.run_once();
    CHECK(log == std::vector<int>{1, 2, 3});

    scheduler.run_once();
    CHECK(log == std::vector<int>{1, 2, 3, 1, 2, 3});
}

TEST_CASE("Scheduler: при переполнении таблицы add_task отказывает, лишняя задача не участвует в обходе") {
    std::vector<int> log;
    LoggingTask a(1, log);
    LoggingTask b(2, log);
    LoggingTask rejected(99, log);

    core::Scheduler<2> scheduler;
    CHECK(scheduler.capacity() == 2);
    REQUIRE(scheduler.add_task(a));
    REQUIRE(scheduler.add_task(b));

    CHECK_FALSE(scheduler.add_task(rejected));
    CHECK(scheduler.task_count() == 2);

    scheduler.run_once();
    CHECK(log == std::vector<int>{1, 2});
}
