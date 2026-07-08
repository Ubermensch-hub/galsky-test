add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

find_package(Threads REQUIRED)

add_executable(unit_tests
    test_main.cpp
    core/test_ring_buffer.cpp
    core/test_scheduler.cpp
    modules/test_sensors.cpp
)
# platform_impl нужен тесту сенсоров: модуль сам читает часы платформы
target_link_libraries(unit_tests PRIVATE core sensors platform_impl doctest Threads::Threads)

add_test(NAME unit_tests COMMAND unit_tests)
