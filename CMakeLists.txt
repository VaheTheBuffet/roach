cmake_minimum_required(VERSION 3.16)

project(roach LANGUAGES C CXX)

option(ROACH_ENABLE_AVX "Build the AVX ray tracer variant" ON)
option(ROACH_USE_NATIVE_ARCH "Pass -march=native when compiling the AVX build" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(ROACH_SOURCES
    main.cpp
    ctx.cpp
    video.cpp
    cjson/cJSON.c
)

if(ROACH_ENABLE_AVX)
    list(APPEND ROACH_SOURCES ray_traceravx.cpp)
else()
    list(APPEND ROACH_SOURCES ray_tracer.cpp)
endif()

add_executable(roach ${ROACH_SOURCES})

target_include_directories(roach PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(ROACH_ENABLE_AVX AND ROACH_USE_NATIVE_ARCH)
    target_compile_options(roach PRIVATE -march=native)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Release")
    target_compile_definitions(roach PRIVATE NDEBUG)
    target_compile_options(roach PRIVATE -O4)
else()
    target_compile_options(roach PRIVATE -g -ggdb)
endif()

message(STATUS "Configured roach build")
message(STATUS "  AVX: ${ROACH_ENABLE_AVX}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
