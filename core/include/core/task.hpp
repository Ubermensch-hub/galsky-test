#pragma once

namespace core {

/* Задача для работы в системе с кооперативным планированием
 * Реализация на общем стеке вызовов внутри цикла планировщика
 * Обеспечивает детерминированность процессов с низкими накладными расходами
*/
class ITask {
public:
    virtual ~ITask() = default;
    virtual void tick() = 0;
};

} // namespace core
