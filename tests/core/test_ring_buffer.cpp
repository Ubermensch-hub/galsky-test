#include <doctest/doctest.h>

#include "core/ring_buffer.hpp"

#include <thread>
#include <vector>

TEST_CASE("RingBufferSpsc: пустой буфер не отдаёт элементы") {
    core::RingBufferSpsc<int, 4> buffer;
    int value = 0;
    CHECK_FALSE(buffer.try_pop(value));
}

TEST_CASE("RingBufferSpsc: push/pop сохраняют порядок (FIFO)") {
    core::RingBufferSpsc<int, 4> buffer;
    REQUIRE(buffer.try_push(1));
    REQUIRE(buffer.try_push(2));
    REQUIRE(buffer.try_push(3));

    int value = 0;
    REQUIRE(buffer.try_pop(value));
    CHECK(value == 1);
    REQUIRE(buffer.try_pop(value));
    CHECK(value == 2);
    REQUIRE(buffer.try_pop(value));
    CHECK(value == 3);
    CHECK_FALSE(buffer.try_pop(value));
}

TEST_CASE("RingBufferSpsc: доступно Capacity - 1 слотов, дальше push отклоняется") {
    core::RingBufferSpsc<int, 4> buffer; // полезная ёмкость == 3
    CHECK(buffer.capacity() == 3);
    CHECK(buffer.try_push(1));
    CHECK(buffer.try_push(2));
    CHECK(buffer.try_push(3));
    CHECK_FALSE(buffer.try_push(4)); // буфер полон
}

TEST_CASE("RingBufferSpsc: корректно переживает несколько оборотов индекса") {
    core::RingBufferSpsc<int, 4> buffer; // полезная ёмкость == 3
    int value = 0;
    for (int round = 0; round < 10; ++round) {
        REQUIRE(buffer.try_push(round * 10 + 1));
        REQUIRE(buffer.try_push(round * 10 + 2));
        REQUIRE(buffer.try_pop(value));
        CHECK(value == round * 10 + 1);
        REQUIRE(buffer.try_pop(value));
        CHECK(value == round * 10 + 2);
    }
}

TEST_CASE("RingBufferSpsc: конкурентные производитель и потребитель не теряют "
          "и не переставляют элементы") {
    core::RingBufferSpsc<int, 1024> buffer;
    constexpr int kItems = 200000;

    std::thread producer([&] {
        for (int i = 0; i < kItems; ++i) {
            while (!buffer.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::vector<int> received;
    received.reserve(kItems);
    int value = 0;
    while (static_cast<int>(received.size()) < kItems) {
        if (buffer.try_pop(value)) {
            received.push_back(value);
        }
    }
    producer.join();

    REQUIRE(received.size() == kItems);
    bool in_order = true;
    for (int i = 0; i < kItems; ++i) {
        if (received[static_cast<std::size_t>(i)] != i) {
            in_order = false;
            break;
        }
    }
    CHECK(in_order);
}
