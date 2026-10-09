cmake_minimum_required(VERSION 4.0)
include(FetchContent)

project(dsl)

set(CMAKE_CXX_STANDARD 20)

add_executable(dsl)

if (APPLE)
    target_link_libraries(dsl PUBLIC "-framework CoreFoundation" "-framework CoreServices")
endif ()

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../dvk ${CMAKE_CURRENT_BINARY_DIR}/dvk)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
target_link_libraries(dsl
    PRIVATE
        glfw
        glm::glm
        dvk
        assimp::assimp
        imgui::imgui
)

target_include_directories(dsl PRIVATE third-party)

target_sources(dsl
    PRIVATE
        src/main.cpp
        src/Platform.cpp
        src/Window.cpp
        src/Game.cpp
        src/Provider.cpp
        src/utils/RNG.cpp
        src/TempImageLoader.cpp
        src/FlyCamera.cpp
        src/Skybox.cpp
        src/Node.cpp
        src/PostProcessing.cpp
        src/Vehicle.cpp
        src/Terrain.cpp
        src/TempModelLoader.cpp
        src/HotReload.cpp
        src/ShaderHotReload.cpp
        src/utils/InputStream.cpp
        src/utils/GrowingBuffer.cpp
        src/components/InstanceComponent.cpp
        src/Renderer.cpp
        src/Model.cpp
        src/Gui.cpp
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
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
        include/Skybox.hpp
        include/PostProcessing.hpp
        include/Node.hpp
        include/Vehicle.hpp
        include/Terrain.hpp
        include/TempModelLoader.hpp
        include/HotReload.hpp
        include/ShaderHotReload.hpp
        include/utils/InputStream.hpp
        include/utils/Serialization.hpp
        include/utils/GrowingBuffer.hpp
        include/components/InstanceComponent.hpp
        include/Renderer.hpp
        include/Model.hpp
        include/Gui.hpp
)

target_include_directories(dsl
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>
)
