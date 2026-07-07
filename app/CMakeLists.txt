add_executable(tracker_app main.cpp)
target_link_libraries(tracker_app PRIVATE core platform_impl modules)
