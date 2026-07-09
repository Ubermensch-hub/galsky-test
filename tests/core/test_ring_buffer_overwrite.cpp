#include <doctest/doctest.h>

#include "core/ring_buffer_overwrite.hpp"

TEST_CASE("RingBufferOverwrite: пустое кольцо не отдаёт элементы") {
    core::RingBufferOverwrite<int, 4> ring;
    CHECK(ring.empty());
    CHECK(ring.size() == 0);
    int v = 0;
    CHECK_FALSE(ring.pop(v));
}

TEST_CASE("RingBufferOverwrite: FIFO и доступна вся ёмкость без жертвенного слота") {
    core::RingBufferOverwrite<int, 4> ring;
    CHECK(ring.capacity() == 4);
    CHECK_FALSE(ring.push(1));
    CHECK_FALSE(ring.push(2));
    CHECK_FALSE(ring.push(3));
    CHECK_FALSE(ring.push(4)); // в отличие от SPSC влезает ровно Capacity
    CHECK(ring.full());
    CHECK(ring.size() == 4);

    int v = 0;
    for (int expected = 1; expected <= 4; ++expected) {
        REQUIRE(ring.pop(v));
        CHECK(v == expected);
    }
    CHECK_FALSE(ring.pop(v));
}

TEST_CASE("RingBufferOverwrite: переполнение вытесняет старейшие записи") {
    core::RingBufferOverwrite<int, 4> ring;
    int evictions = 0;
    for (int i = 1; i <= 6; ++i) {
        if (ring.push(i)) {
            ++evictions;
        }
    }
    CHECK(evictions == 2); // 1 и 2 вытеснены
    CHECK(ring.size() == 4);

    int v = 0;
    for (int expected = 3; expected <= 6; ++expected) {
        REQUIRE(ring.pop(v));
        CHECK(v == expected);
    }
    CHECK_FALSE(ring.pop(v));
}

TEST_CASE("RingBufferOverwrite: несколько оборотов с чередованием push/pop") {
    core::RingBufferOverwrite<int, 4> ring;
    int v = 0;
    for (int round = 0; round < 10; ++round) {
        CHECK_FALSE(ring.push(round * 10 + 1));
        CHECK_FALSE(ring.push(round * 10 + 2));
        REQUIRE(ring.pop(v));
        CHECK(v == round * 10 + 1);
        REQUIRE(ring.pop(v));
        CHECK(v == round * 10 + 2);
    }
    CHECK(ring.empty());
}
