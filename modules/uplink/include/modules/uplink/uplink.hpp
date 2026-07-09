#pragma once

#include "core/task.hpp"
#include "msg/record_source.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

// Модуль 6 - Передача данных на сервер (Накопление и отправка пакетов)
namespace modules::uplink {

// Транспорт канала связи. (заглушка)
class ILinkTransport {
public:
    virtual ~ILinkTransport() = default;
    // true - пакет принят каналом
    virtual bool send(const msg::Record* records, std::size_t count) = 0;
};

// Кооперативная задача, по сетке периодов забирает записи из источника, копит и отправляет.
// Неудачная отправка не теряет записи, batch удерживается до следующей попытки
class UplinkTask : public core::ITask {
public:
    static constexpr std::size_t kBatchSize = 8;

    UplinkTask(msg::IRecordSource& source, ILinkTransport& transport, uint32_t period_ms)
        : source_(&source), transport_(&transport), period_ms_(period_ms) {}

    void tick(uint32_t now_ms) override {
        if (!started_) {
            started_ = true;
            next_send_ms_ = now_ms;
        }

        if (static_cast<int32_t>(now_ms - next_send_ms_) < 0) {
            return;
        }
        next_send_ms_ += period_ms_;

        // Добор после сбоя связи, хвост прошлого пакета уже внутри
        while (batch_count_ < kBatchSize) {
            msg::Record record;
            if (!source_->pop_record(record)) {
                break;
            }
            batch_[batch_count_++] = record;
        }
        if (batch_count_ == 0) {
            return;
        }

        if (transport_->send(batch_.data(), batch_count_)) {
            sent_records_ += static_cast<uint32_t>(batch_count_);
            ++sent_packets_;
            batch_count_ = 0;
        } else {
            ++failed_sends_; // записи остаются в пакете
        }
    }

    std::size_t pending_records() const { return batch_count_; }
    uint32_t sent_packets() const { return sent_packets_; }
    uint32_t sent_records() const { return sent_records_; }
    uint32_t failed_sends() const { return failed_sends_; }

private:
    msg::IRecordSource* source_;
    ILinkTransport* transport_;
    uint32_t period_ms_;
    uint32_t next_send_ms_{0};
    bool started_{false};
    std::array<msg::Record, kBatchSize> batch_{};
    std::size_t batch_count_{0};
    uint32_t sent_packets_{0};
    uint32_t sent_records_{0};
    uint32_t failed_sends_{0};
};

} // namespace modules::uplink
