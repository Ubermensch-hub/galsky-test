#include "platform/clock_hal.hpp"

#include <cstdio>

//Точка входа, пишет время с запуска платформы

int main() {
    std::printf("tracker skeleton up, platform clock = %u ms\n", platform::now_ms());
    return 0;
}
