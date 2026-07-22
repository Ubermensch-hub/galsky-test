#pragma once

#include <cstdint>

// Неблокирующий TCP-сервер для стриминга строк телеметрии (одна строка = одно
// сообщение NDJSON). Реализуется на этапе линковки; в mcu_stub -- заглушка
namespace platform {

// Открыть порт на 127.0.0.1; false -- сеть недоступна
bool net_listen(uint16_t port);

// Принять нового клиента / отбросить умершего; вызывать в главном цикле
void net_poll();

// false -- клиента нет или канал занят, строка отбрасывается
bool net_send_line(const char* line);

void net_shutdown();

} // namespace platform
