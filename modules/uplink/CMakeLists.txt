add_library(uplink INTERFACE)
target_include_directories(uplink INTERFACE include)
target_link_libraries(uplink INTERFACE core msg)
