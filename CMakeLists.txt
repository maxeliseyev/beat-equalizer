cmake_minimum_required(VERSION 3.22)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" BEAT_EQUALIZER_VERSION)
string(STRIP "${BEAT_EQUALIZER_VERSION}" BEAT_EQUALIZER_VERSION)
if(NOT BEAT_EQUALIZER_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must be major.minor.patch, got '${BEAT_EQUALIZER_VERSION}'")
endif()

project(BeatEqualizer
    VERSION ${BEAT_EQUALIZER_VERSION}
    DESCRIPTION "Multi-mic drum alignment plugin"
    LANGUAGES C CXX)

message(STATUS "Beat Equalizer ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

include(cmake/Dependencies.cmake)

add_subdirectory(src/dsp)
add_subdirectory(src/doc)
add_subdirectory(src/plugin)

enable_testing()
add_subdirectory(tests)
