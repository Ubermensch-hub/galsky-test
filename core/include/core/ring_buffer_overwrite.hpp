#pragma once

#include <array>
#include <cstddef>

namespace core {

/* Кольцо с вытеснением старейших записей (drop-old) для одного контекста
* исполнения: без атомиков и блокировок. Дополняет RingBufferSpsc, там
* вытеснение со стороны производителя нарушало бы lock-free-инвариант,
 здесь конкуренции нет и доступна вся ёмкость без жертвенного слота*/
template <typename T, std::size_t Capacity>
class RingBufferOverwrite {
    static_assert(Capacity > 0, "Capacity must be positive");

public:
    RingBufferOverwrite() = default;
    RingBufferOverwrite(const RingBufferOverwrite&) = delete;
    RingBufferOverwrite& operator=(const RingBufferOverwrite&) = delete;

    // true - кольцо было полно и старейшая запись вытеснена
    bool push(const T& item) {
        storage_[write_] = item;
        write_ = next(write_);
        if (size_ == Capacity) {
            read_ = next(read_); // перезаписали самую старую
            return true;
        }
        ++size_;
        return false;
    }

    // Отдаёт старейшую запись
    bool pop(T& out) {
        if (size_ == 0) {
            return false;
        }
        out = storage_[read_];
        read_ = next(read_);
        --size_;
        return true;
    }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Capacity; }
    static constexpr std::size_t capacity() { return Capacity; }

private:
    static std::size_t next(std::size_t i) { return (i + 1 == Capacity) ? 0 : i + 1; }

    std::array<T, Capacity> storage_{};
    std::size_t write_{0};
    std::size_t read_{0};
    std::size_t size_{0};
};

} // namespace core
