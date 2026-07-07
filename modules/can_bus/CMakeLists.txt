add_library(can_bus INTERFACE)
target_include_directories(can_bus INTERFACE include)
target_link_libraries(can_bus INTERFACE core)
