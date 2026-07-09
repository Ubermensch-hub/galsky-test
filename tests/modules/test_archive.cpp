#include <doctest/doctest.h>

#include "modules/archive/archive.hpp"

#include <utility>

using modules::archive::ArchiveTask;

namespace {

msg::ZoneEvent ev(uint32_t ts) {
    msg::ZoneEvent e{};
    e.zone_id = 1;
    e.type = msg::ZoneEventType::Entered;
    e.timestamp_ms = ts;
    return e;
}

} // namespace

TEST_CASE("ArchiveTask: события становятся записями со сквозной нумерацией") {
    msg::ZoneEventTopic events;
    msg::ZoneEventTopic::Subscription sub;
    REQUIRE(events.subscribe(sub));
    ArchiveTask task(std::move(sub));

    events.publish(ev(100));
    events.publish(ev(200));
    task.tick(0);
    CHECK(task.size() == 2);

    msg::Record record;
    REQUIRE(task.pop_record(record));
    CHECK(record.seq == 1);
    CHECK(record.kind == msg::RecordKind::ZoneEvent);
    CHECK(record.event.timestamp_ms == 100);

    REQUIRE(task.pop_record(record));
    CHECK(record.seq == 2);
    CHECK(record.event.timestamp_ms == 200);

    CHECK_FALSE(task.pop_record(record));
    CHECK(task.size() == 0);
}

TEST_CASE("ArchiveTask: переполнение вытесняет старейшие записи, вытеснения считаются") {
    msg::ZoneEventTopic events;
    msg::ZoneEventTopic::Subscription sub;
    REQUIRE(events.subscribe(sub));
    ArchiveTask task(std::move(sub));

    // На 2 события больше ёмкости; тик после каждой публикации,
    // чтобы не упереться в ёмкость кольца подписки
    for (uint32_t i = 0; i < ArchiveTask::kCapacity + 2; ++i) {
        events.publish(ev(i));
        task.tick(0);
    }

    CHECK(task.size() == ArchiveTask::kCapacity);
    CHECK(task.evicted_count() == 2);

    // Записи seq 1 и 2 вытеснены: потребитель видит разрыв нумерации
    msg::Record record;
    REQUIRE(task.pop_record(record));
    CHECK(record.seq == 3);
    CHECK(record.event.timestamp_ms == 2);
}
