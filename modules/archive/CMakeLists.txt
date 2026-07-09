add_library(archive INTERFACE)
target_include_directories(archive INTERFACE include)
target_link_libraries(archive INTERFACE core msg)
