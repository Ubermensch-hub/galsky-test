add_library(sensors INTERFACE)
target_include_directories(sensors INTERFACE include)
target_link_libraries(sensors INTERFACE core platform)
