add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

find_package(Threads REQUIRED)

add_executable(unit_tests
    test_main.cpp
    core/test_ring_buffer.cpp
)
target_link_libraries(unit_tests PRIVATE core doctest Threads::Threads)

add_test(NAME unit_tests COMMAND unit_tests)
