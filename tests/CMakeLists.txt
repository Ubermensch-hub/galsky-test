add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

add_executable(unit_tests test_main.cpp)
target_link_libraries(unit_tests PRIVATE core doctest)

add_test(NAME unit_tests COMMAND unit_tests)
