cmake_minimum_required(VERSION 3.22)

project(BeatEqualizer
    VERSION 0.1.0
    DESCRIPTION "Multi-mic drum alignment plugin"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

include(cmake/Dependencies.cmake)

add_subdirectory(src/dsp)
add_subdirectory(src/plugin)

enable_testing()
add_subdirectory(tests)
