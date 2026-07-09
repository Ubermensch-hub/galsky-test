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


    // Строка должна жить дольше планировщика
    bool add_task(ITask& task, const char* name = "") {
        if (count_ == MaxTasks) {
            return false;
        }
        slots_[count_++] = {&task, name, 0};
        return true;
    }

    /* Один круг round-robin: каждая задача получает ровно один tick() в порядке
     * регистрации. Текущее время читает внешний цикл и подаёт сюда -- core не
     зависит от часов платформы */
    void run_once(uint32_t now_ms) {
        for (std::size_t i = 0; i < count_; ++i) {
            slots_[i].task->tick(now_ms);
            ++slots_[i].ticks;
        }
    }

    std::size_t task_count() const { return count_; }
    static constexpr std::size_t capacity() { return MaxTasks; }

    // Статистика для диагностики, предусловие: i < task_count()
    const char* task_name(std::size_t i) const { return slots_[i].name; }
    uint32_t task_ticks(std::size_t i) const { return slots_[i].ticks; }

private:
    struct Slot {
        ITask* task;
        const char* name;
        uint32_t ticks;
    };

    std::array<Slot, MaxTasks> slots_{};
    std::size_t count_{0};
};

} // namespace core
