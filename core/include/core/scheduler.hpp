#pragma once

#include "core/task.hpp"

#include <array>
#include <cstddef>

namespace core {

// Кооперативный планировщик на статической таблице задач.
template <std::size_t MaxTasks>
class Scheduler {
    static_assert(MaxTasks > 0, "MaxTasks must be positive");

public:
    Scheduler() = default;
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;


    bool add_task(ITask& task) {
        if (count_ == MaxTasks) {
            return false;
        }
        tasks_[count_++] = &task;
        return true;
    }

    /* Один круг round-robin: каждая задача получает ровно один tick() в порядке
     * регистрации. Текущее время читает внешний цикл и подаёт сюда -- core не
     зависит от часов платформы */
    void run_once(uint32_t now_ms) {
        for (std::size_t i = 0; i < count_; ++i) {
            tasks_[i]->tick(now_ms);
        }
    }

    std::size_t task_count() const { return count_; }
    static constexpr std::size_t capacity() { return MaxTasks; }

private:
    std::array<ITask*, MaxTasks> tasks_{};
    std::size_t count_{0};
};

} // namespace core
