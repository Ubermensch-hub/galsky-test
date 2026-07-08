#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <utility>

namespace core {

// Кольцевой буфер SPSC (Single-producer, single-consumer) без блокировок и кучи.
// Безопасен для передачи данных из ISR-подобного источника в кооперативную задачу:
// try_push вызывается только из контекста производителя, try_pop -- только из
// контекста потребителя; конкурентные производители/потребители не поддерживаются.
template <typename T, std::size_t Capacity>
class RingBufferSpsc {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

public:
    RingBufferSpsc() = default;
    RingBufferSpsc(const RingBufferSpsc&) = delete;
    RingBufferSpsc& operator=(const RingBufferSpsc&) = delete;

    bool try_push(const T& item) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next_head = (head + 1) & kMask;
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // буфер полон
        }
        storage_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    bool try_push(T&& item) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next_head = (head + 1) & kMask;
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // буфер полон
        }
        storage_[head] = std::move(item);
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // буфер пуст
        }
        out = std::move(storage_[tail]);
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    static constexpr std::size_t capacity() { return Capacity - 1; }

    // Приблизительный размер: годится для диагностики/логов, не для управления потоком
    std::size_t size_approx() const {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & kMask;
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    std::array<T, Capacity> storage_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

} // namespace core
