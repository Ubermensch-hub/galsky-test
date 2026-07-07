add_library(platform_impl STATIC
    clock_hal_mcu.cpp
)
target_link_libraries(platform_impl PUBLIC platform)
