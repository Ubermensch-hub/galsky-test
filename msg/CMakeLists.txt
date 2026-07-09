add_library(msg INTERFACE)
target_include_directories(msg INTERFACE include)
target_link_libraries(msg INTERFACE core)
