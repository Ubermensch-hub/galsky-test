#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "core/task.hpp"

namespace {

// Дымовой тест для проверки правильности определения платформы на этапе линковки

class CountingTask : public core::ITask {
public:
    void tick() override { ++ticks; }
    int ticks = 0;
};

} // namespace

TEST_CASE("ITask implementations can be ticked") {
    CountingTask task;
    task.tick();
    task.tick();
    CHECK(task.ticks == 2);
}
