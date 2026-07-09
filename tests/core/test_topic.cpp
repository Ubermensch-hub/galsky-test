#include <doctest/doctest.h>

#include "core/topic.hpp"

#include <utility>
#include <vector>

using TestTopic = core::Topic<int, 2, 4>; // 2 подписчика, полезная ёмкость кольца = 3

TEST_CASE("Topic: публикация без подписчиков безопасна и без потерь") {
    TestTopic topic;
    CHECK(topic.subscriber_count() == 0);
    CHECK(topic.publish(42) == 0);
}

TEST_CASE("Topic: каждый подписчик получает собственную копию всех сообщений") {
    TestTopic topic;
    TestTopic::Subscription a;
    TestTopic::Subscription b;
    REQUIRE(topic.subscribe(a));
    REQUIRE(topic.subscribe(b));

    CHECK(topic.publish(1) == 0);
    CHECK(topic.publish(2) == 0);

    int v = 0;
    REQUIRE(a.try_pop(v));
    CHECK(v == 1);
    REQUIRE(a.try_pop(v));
    CHECK(v == 2);
    CHECK_FALSE(a.try_pop(v));

    // b читает независимо: вычитка a не тронула его кольцо
    REQUIRE(b.try_pop(v));
    CHECK(v == 1);
    REQUIRE(b.try_pop(v));
    CHECK(v == 2);
    CHECK_FALSE(b.try_pop(v));
}

TEST_CASE("Topic: переполнение у медленного подписчика не влияет на быстрого") {
    TestTopic topic;
    TestTopic::Subscription fast;
    TestTopic::Subscription slow;
    REQUIRE(topic.subscribe(fast));
    REQUIRE(topic.subscribe(slow));

    int v = 0;
    std::vector<int> fast_got;
    std::size_t drops = 0;
    for (int i = 1; i <= 5; ++i) {
        drops += topic.publish(i);
        while (fast.try_pop(v)) {
            fast_got.push_back(v); // быстрый вычитывает сразу
        }
    }

    CHECK(fast_got == std::vector<int>{1, 2, 3, 4, 5});
    CHECK(drops == 2); // 4 и 5 не влезли в кольцо медленного

    // медленный потерял новые сообщения, а не старые
    std::vector<int> slow_got;
    while (slow.try_pop(v)) {
        slow_got.push_back(v);
    }
    CHECK(slow_got == std::vector<int>{1, 2, 3});
}

TEST_CASE("Topic: отказ при исчерпании слотов подписки") {
    TestTopic topic;
    TestTopic::Subscription a;
    TestTopic::Subscription b;
    TestTopic::Subscription rejected;
    REQUIRE(topic.subscribe(a));
    REQUIRE(topic.subscribe(b));

    CHECK_FALSE(topic.subscribe(rejected));
    CHECK_FALSE(rejected.valid());
    CHECK(topic.subscriber_count() == 2);

    int v = 0;
    CHECK_FALSE(rejected.try_pop(v)); // невалидный хэндл безопасен
}

TEST_CASE("Topic: Подписка переносима, источник инвалидируется") {
    TestTopic topic;
    TestTopic::Subscription a;
    REQUIRE(topic.subscribe(a));
    CHECK(topic.publish(7) == 0);

    TestTopic::Subscription moved = std::move(a);
    CHECK_FALSE(a.valid());
    CHECK(moved.valid());

    int v = 0;
    REQUIRE(moved.try_pop(v));
    CHECK(v == 7);
}
