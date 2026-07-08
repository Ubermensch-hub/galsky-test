#pragma once

#include "core/task.hpp"

#include <cstdint>

// Модуль 7 - Обработка датчиков/АЦП (Дискретные и аналоговые входы)
namespace modules::sensors {

// Источник значения датчика. Реализация внедряется снаружи
class ISampleSource {
public:
    virtual ~ISampleSource() = default;
    virtual uint16_t read() = 0;
};

// Кооперативная задача: опрашивает источник с заданным периодом.
class SensorPollTask : public core::ITask {
public:
    SensorPollTask(ISampleSource& source, uint32_t period_ms)
        : source_(&source), period_ms_(period_ms) {}

    void tick(uint32_t now_ms) override {
        if (!started_) {
            started_ = true;
            next_due_ms_ = now_ms;
        }
        // Сравнение через знаковую разность корректно при переполнении uint32_t
        if (static_cast<int32_t>(now_ms - next_due_ms_) >= 0) {
            last_ = source_->read();
            ++count_;
            // Фиксированная сетка периодов без накопления дрейфа
            next_due_ms_ += period_ms_;
        }
    }

    uint16_t last_sample() const { return last_; }
    uint32_t sample_count() const { return count_; }

private:
    ISampleSource* source_;
    uint32_t period_ms_;
    uint32_t next_due_ms_{0};
    bool started_{false};
    uint16_t last_{0};
    uint32_t count_{0};
};

} // namespace modules::sensors
