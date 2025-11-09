cmake_minimum_required(VERSION 3.22)
project(ElementalPong LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(raylib CONFIG REQUIRED)

add_executable(elemental_pong
    src/main.cpp
    src/AudioManager.cpp
    src/InstructionsScreen.cpp
    src/ElementalGame.cpp
)
target_link_libraries(elemental_pong PRIVATE raylib)

if (APPLE)
    target_link_libraries(elemental_pong PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

