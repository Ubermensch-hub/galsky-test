add_library(platform_impl STATIC
    clock_hal_desktop.cpp
    net_hal_desktop.cpp
)
target_link_libraries(platform_impl PUBLIC platform)
if(WIN32)
    target_link_libraries(platform_impl PRIVATE ws2_32)
endif()
