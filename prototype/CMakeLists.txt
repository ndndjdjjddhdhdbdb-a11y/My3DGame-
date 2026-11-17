cmake_minimum_required(VERSION 3.16)

project(BoltSurvivalPrototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(raylib REQUIRED)

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
