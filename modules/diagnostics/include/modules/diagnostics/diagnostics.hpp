#pragma once

#include "core/task.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

// Модуль 10 - Диагностика, лог, watchdog (Сбор статистики, контроль состояния модулей)
namespace modules::diagnostics {

// Проба здоровья: счётчик прогресса модуля и крайний срок его продвижения.
// Чтение через указатель на функцию + объект
struct HealthProbe {
    const char* name;
    uint32_t deadline_ms;
    uint32_t (*read)(const void* module);
    const void* module;
};

/* Программный watchdog: следит, что счётчики прогресса модулей продвигаются.
* Замерший модуль объявляется stalled (однократно) и сообщается через колбэк
* возобновление прогресса снимает диагноз. На железе system_healthy()
* гейтирует сброс аппаратного watchdog.
* полное зависание tick() способен поймать только он */
class WatchdogTask : public core::ITask {
public:
    static constexpr std::size_t kMaxProbes = 16;
    using StallCallback = void (*)(const char* probe_name, uint32_t now_ms, void* user);

    explicit WatchdogTask(StallCallback on_stall = nullptr, void* user = nullptr)
        : on_stall_(on_stall), user_(user) {}

    // false - таблица заполнена
    bool add_probe(const HealthProbe& probe) {
        if (count_ == kMaxProbes) {
            return false;
        }
        slots_[count_++] = {probe, 0, 0, false, false};
        return true;
    }

    void tick(uint32_t now_ms) override {
        for (std::size_t i = 0; i < count_; ++i) {
            Slot& slot = slots_[i];
            const uint32_t value = slot.probe.read(slot.probe.module);
            if (!slot.initialized) {
                slot.initialized = true;
                slot.last_value = value;
                slot.last_change_ms = now_ms;
                continue;
            }
            if (value != slot.last_value) {
                slot.last_value = value;
                slot.last_change_ms = now_ms;
                if (slot.stalled) {
                    slot.stalled = false;
                    --stalled_now_;
                }
                continue;
            }

            const uint32_t elapsed = now_ms - slot.last_change_ms;
            if (!slot.stalled && elapsed > slot.probe.deadline_ms) {
                slot.stalled = true;
                ++stalled_now_;
                ++stall_events_;
                if (on_stall_ != nullptr) {
                    on_stall_(slot.probe.name, now_ms, user_);
                }
            }
        }
    }

    bool system_healthy() const { return stalled_now_ == 0; }
    uint32_t stall_events() const { return stall_events_; }
    std::size_t probe_count() const { return count_; }

private:
    struct Slot {
        HealthProbe probe;
        uint32_t last_value;
        uint32_t last_change_ms;
        bool initialized;
        bool stalled;
    };

    StallCallback on_stall_;
    void* user_;
    std::array<Slot, kMaxProbes> slots_{};
    std::size_t count_{0};
    std::size_t stalled_now_{0};
    uint32_t stall_events_{0};
};

} // namespace modules::diagnostics
