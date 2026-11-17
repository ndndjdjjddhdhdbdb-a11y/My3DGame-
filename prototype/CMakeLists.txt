cmake_minimum_required(VERSION 3.16)

project(BoltSurvivalPrototype LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Если raylib не установлена в системе, CMake скачает её сам.
include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(raylib)

add_executable(BoltSurvival
    src/main.cpp
    src/Game.cpp
    src/Player.cpp
    src/World.cpp
)

target_include_directories(BoltSurvival PRIVATE src)
target_link_libraries(BoltSurvival PRIVATE raylib)

if(MSVC)
    target_compile_options(BoltSurvival PRIVATE /W4)
else()
    target_compile_options(BoltSurvival PRIVATE -Wall -Wextra -Wpedantic)
endif()
