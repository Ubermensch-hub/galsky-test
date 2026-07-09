#pragma once

#include "core/ring_buffer.hpp"

#include <array>
#include <cstddef>

namespace core {

/* Канал один-ко-многим поверх SPSC-колец: у каждого подписчика собственное
* кольцо, publish кладёт сообщение во все. Медленный подписчик теряет только
свои сообщения (drop-new) и не блокирует ни производителя, ни остальных.*/
template <typename T, std::size_t MaxSubscribers, std::size_t QueueCapacity>
class Topic {
    static_assert(MaxSubscribers > 0, "MaxSubscribers must be positive");

public:
    Topic() = default;
    Topic(const Topic&) = delete;
    Topic& operator=(const Topic&) = delete;

    // Два потребителя одного кольца исключены на уровне типов
    class Subscription {
    public:
        Subscription() = default;
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        Subscription(Subscription&& other) noexcept
            : topic_(other.topic_), index_(other.index_) {
            other.topic_ = nullptr;
        }
        Subscription& operator=(Subscription&& other) noexcept {
            topic_ = other.topic_;
            index_ = other.index_;
            other.topic_ = nullptr;
            return *this;
        }

        bool valid() const { return topic_ != nullptr; }

        // Вызывается только из контекста подписчика
        bool try_pop(T& out) {
            return topic_ != nullptr && topic_->queues_[index_].try_pop(out);
        }

    private:
        friend class Topic;
        Topic* topic_ = nullptr;
        std::size_t index_ = 0;
    };


    bool subscribe(Subscription& out) {
        if (count_ == MaxSubscribers) {
            return false;
        }
        out.topic_ = this;
        out.index_ = count_++;
        return true;
    }

    // Возвращает число подписчиков, потерявших сообщение из-за полного кольца
    std::size_t publish(const T& msg) {
        std::size_t dropped = 0;
        for (std::size_t i = 0; i < count_; ++i) {
            if (!queues_[i].try_push(msg)) {
                ++dropped;
            }
        }
        return dropped;
    }

    std::size_t subscriber_count() const { return count_; }
    static constexpr std::size_t max_subscribers() { return MaxSubscribers; }
    static constexpr std::size_t queue_capacity() {
        return RingBufferSpsc<T, QueueCapacity>::capacity();
    }

private:
    std::array<RingBufferSpsc<T, QueueCapacity>, MaxSubscribers> queues_;
    std::size_t count_{0};
};

} // namespace core
