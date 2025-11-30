cmake_minimum_required(VERSION 4.0)
project(zombies)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

add_executable(zombies)

find_package(raylib CONFIG REQUIRED)
target_link_libraries(zombies PRIVATE raylib)

target_sources(zombies
    PRIVATE
        src/main.cpp
        src/Application.cpp
        src/Terrain.cpp
        src/Utils/RNG.cpp
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/Application.hpp
        include/Terrain.hpp
        include/Utils/RNG.hpp
        include/Utils/Paths.hpp
)

target_include_directories(zombies
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>
)