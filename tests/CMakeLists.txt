add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

find_package(Threads REQUIRED)

add_executable(unit_tests
    test_main.cpp
    core/test_ring_buffer.cpp
    core/test_ring_buffer_overwrite.cpp
    core/test_scheduler.cpp
    core/test_topic.cpp
    modules/test_sensors.cpp
    modules/test_geofence.cpp
    modules/test_archive.cpp
    modules/test_uplink.cpp
    modules/test_gps_sim.cpp
    modules/test_diagnostics.cpp
    integration/test_end_to_end.cpp
)
target_link_libraries(unit_tests PRIVATE core sensors geofence_engine archive uplink gps diagnostics doctest Threads::Threads)

add_test(NAME unit_tests COMMAND unit_tests)
