#pragma once

#include "core/ring_buffer_overwrite.hpp"
#include "core/task.hpp"
#include "msg/record_source.hpp"
#include "msg/topics.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

// Модуль 5 - Архив (Накопление записей с ограничением по объёму)
namespace modules::archive {

/* Кооперативная задача, которая превращает события в нумерованные записи и хранит их
* в кольце с вытеснением старейших. Потребитель забирает записи pop_record() напрямую
 обе задачи живут в одном кооперативном контексте, синхронизация не нужна*/
class ArchiveTask : public core::ITask, public msg::IRecordSource {
public:
    static constexpr std::size_t kCapacity = 32;

    explicit ArchiveTask(msg::ZoneEventTopic::Subscription&& events)
        : events_(std::move(events)) {}

    void tick(uint32_t) override {
        msg::ZoneEvent event;
        while (events_.try_pop(event)) { // цикл ограничен ёмкостью кольца подписки
            msg::Record record{};
            record.seq = next_seq_++;
            record.kind = msg::RecordKind::ZoneEvent;
            record.event = event;
            if (ring_.push(record)) {
                ++evicted_;
            }
        }
    }

    // Отдаёт старейшую запись
    bool pop_record(msg::Record& out) override { return ring_.pop(out); }

    std::size_t size() const { return ring_.size(); }

    // Вытеснения при переполнении (для диагностики)
    uint32_t evicted_count() const { return evicted_; }

private:
    msg::ZoneEventTopic::Subscription events_;
    core::RingBufferOverwrite<msg::Record, kCapacity> ring_;
    uint32_t next_seq_{1};
    uint32_t evicted_{0};
};

} // namespace modules::archive
