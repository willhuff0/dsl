cmake_minimum_required(VERSION 4.0)
include(FetchContent)

project(zombies)

set(CMAKE_CXX_STANDARD 20)

add_executable(zombies)

find_package(glfw3 3.4 REQUIRED)
#FetchContent_Declare(dvk
#    GIT_REPOSITORY https://github.com/willhuff0/dvk
#    GIT_SHALLOW    ON
#)
#FetchContent_MakeAvailable(dvk)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../dvk ${CMAKE_CURRENT_BINARY_DIR}/dvk)
target_link_libraries(zombies
    PRIVATE
        glfw
        dvk
)

target_sources(zombies
    PRIVATE
        src/main.cpp
        src/Platform.cpp
        src/Window.cpp
        src/Game.cpp
        src/Provider.cpp
        src/utils/RNG.cpp
        src/TempImageLoader.cpp
        src/FlyCamera.cpp
#        src/Terrain.cpp
        src/Skybox.cpp
#        src/Node.cpp
        src/PostProcessing.cpp
#        src/Vehicle.cpp
#        src/MeshRenderer.cpp
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/deps/FastNoiseLite.h
        include/deps/stb_image.h
        include/Platform.hpp
        include/Window.hpp
        include/Game.hpp
        include/Provider.hpp
        include/utils/Paths.hpp
        include/utils/RNG.hpp
        include/utils/ObserverPattern.hpp
        include/DataImage.hpp
        include/TempImageLoader.hpp
        include/FlyCamera.hpp
#        include/Terrain.hpp
        include/Skybox.hpp
        include/PostProcessing.hpp
#        include/Node.hpp
#        include/Vehicle.hpp
#        include/MeshRenderer.hpp
)

target_include_directories(zombies
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>
)