#include <doctest/doctest.h>

#include "modules/uplink/uplink.hpp"

#include <cstddef>
#include <vector>

using modules::uplink::UplinkTask;

namespace {

// Источник с заранее заготовленными записями
class FakeSource : public msg::IRecordSource {
public:
    bool pop_record(msg::Record& out) override {
        if (next_ >= records_.size()) {
            return false;
        }
        out = records_[next_++];
        return true;
    }

    void add(uint32_t seq) {
        msg::Record record{};
        record.seq = seq;
        record.kind = msg::RecordKind::ZoneEvent;
        records_.push_back(record);
    }

private:
    std::vector<msg::Record> records_;
    std::size_t next_{0};
};

// Транспорт-заглушка: журналирует доставленные записи, умеет «терять связь»
class FakeTransport : public modules::uplink::ILinkTransport {
public:
    bool send(const msg::Record* records, std::size_t count) override {
        ++send_calls;
        if (!online) {
            return false;
        }
        for (std::size_t i = 0; i < count; ++i) {
            delivered.push_back(records[i].seq);
        }
        ++packets;
        return true;
    }

    bool online = true;
    int send_calls = 0;
    int packets = 0;
    std::vector<uint32_t> delivered;
};

} // namespace

TEST_CASE("UplinkTask: отправка по сетке периодов, пустой источник не дёргает транспорт") {
    FakeSource source;
    FakeTransport transport;
    UplinkTask task(source, transport, 500);

    source.add(1);
    source.add(2);

    task.tick(1000); // первая попытка сразу
    CHECK(transport.packets == 1);
    CHECK(transport.delivered == std::vector<uint32_t>{1, 2});

    task.tick(1400); // до срока
    CHECK(transport.send_calls == 1);

    task.tick(1500); // срок, но источник пуст -- транспорт не трогаем
    CHECK(transport.send_calls == 1);
    CHECK(task.sent_records() == 2);
}

TEST_CASE("UplinkTask: пакет ограничен kBatchSize, остаток уходит следующим пакетом") {
    FakeSource source;
    FakeTransport transport;
    UplinkTask task(source, transport, 500);

    for (uint32_t seq = 1; seq <= UplinkTask::kBatchSize + 3; ++seq) {
        source.add(seq);
    }

    task.tick(0);
    CHECK(transport.packets == 1);
    CHECK(transport.delivered.size() == UplinkTask::kBatchSize);

    task.tick(500);
    CHECK(transport.packets == 2);
    CHECK(transport.delivered.size() == UplinkTask::kBatchSize + 3);
    CHECK(transport.delivered.back() == UplinkTask::kBatchSize + 3); // порядок сохранён
}

TEST_CASE("UplinkTask: сбой связи удерживает пакет, записи не теряются") {
    FakeSource source;
    FakeTransport transport;
    UplinkTask task(source, transport, 500);

    source.add(1);
    source.add(2);
    transport.online = false;

    task.tick(0);
    CHECK(transport.send_calls == 1);
    CHECK(transport.packets == 0);
    CHECK(task.failed_sends() == 1);
    CHECK(task.pending_records() == 2); // батч удержан

    source.add(3);
    transport.online = true;

    task.tick(500); // дослив после восстановления
    CHECK(transport.packets == 1);
    CHECK(transport.delivered == std::vector<uint32_t>{1, 2, 3}); // хвост + добор
    CHECK(task.pending_records() == 0);
    CHECK(task.sent_records() == 3);
}
