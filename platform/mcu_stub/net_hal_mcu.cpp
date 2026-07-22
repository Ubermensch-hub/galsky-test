#include "platform/net_hal.hpp"

// Сети на целевом МК в этом каркасе нет: телеметрия дашборда -- десктопная
// возможность, канал устройства с миром -- uplink через ILinkTransport
namespace platform {

bool net_listen(uint16_t) { return false; }
void net_poll() {}
bool net_send_line(const char*) { return false; }
void net_shutdown() {}

} // namespace platform
