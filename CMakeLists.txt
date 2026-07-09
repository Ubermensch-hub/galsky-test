cmake_minimum_required(VERSION 3.16)
project(galsky_tracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()

# Исходники в UTF-8 (комментарии на русском): без явного флага MSVC читает
# файлы в системной кодировке и может исказить литералы (C4819)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Selects which HAL implementation is linked in for hardware-facing code. The portable
# sources (core/, modules/) never branch on this or on the target platform -- CMake
# alone decides which platform/*/*.cpp files get compiled in (see platform/CMakeLists.txt).
set(TRACKER_PLATFORM "desktop" CACHE STRING "HAL implementation to link: desktop or mcu_stub")
set_property(CACHE TRACKER_PLATFORM PROPERTY STRINGS desktop mcu_stub)

option(TRACKER_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(core)
add_subdirectory(msg)
add_subdirectory(platform)
add_subdirectory(modules)
add_subdirectory(app)

if(TRACKER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
