cmake_minimum_required(VERSION 3.22.1)
project(haris_game LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffunction-sections -fdata-sections")

add_library(haris_game SHARED
    haris_android_runtime.c
    haris_jni.cpp
)

target_include_directories(haris_game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/haris)
target_link_libraries(haris_game log android m)
target_link_options(haris_game PRIVATE -Wl,--gc-sections)
