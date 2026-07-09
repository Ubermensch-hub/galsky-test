#pragma once

#include "msg/messages.hpp"

namespace msg {

// Источник записей для передатчика (реализуется архивом)
// Вызовы только внутри кооперативного контекста, синхронизации не требует
class IRecordSource {
public:
    virtual ~IRecordSource() = default;
    virtual bool pop_record(Record& out) = 0;
};

} // namespace msg
