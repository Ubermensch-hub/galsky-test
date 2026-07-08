#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "core/task.hpp"

namespace {

// Дымовой тест для проверки правильности определения платформы на этапе линковки

class CountingTask : public core::ITask {
public:
    void tick(uint32_t) override { ++ticks; }
    int ticks = 0;
};

} // namespace

TEST_CASE("ITask: Реализации корректно работают") {
    CountingTask task;
    task.tick(0);
    task.tick(0);
    CHECK(task.ticks == 2);
}
