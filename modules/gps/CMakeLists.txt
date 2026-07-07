add_library(gps INTERFACE)
target_include_directories(gps INTERFACE include)
target_link_libraries(gps INTERFACE core)
