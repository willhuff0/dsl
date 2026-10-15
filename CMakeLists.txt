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
find_package(box3d CONFIG REQUIRED)
target_link_libraries(dsl
    PRIVATE
        glfw
        glm::glm
        dvk
        assimp::assimp
        imgui::imgui
        box3d::box3d
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
        src/terrain/Terrain.cpp
        src/terrain/HeightmapGenerator.cpp
        src/TempModelLoader.cpp
        src/HotReload.cpp
        src/ShaderHotReload.cpp
        src/utils/InputStream.cpp
        src/utils/GrowingBuffer.cpp
        src/utils/UploadQueue.cpp
        src/utils/ChunkedBuffer.cpp
        src/components/InstanceComponent.cpp
        src/Renderer.cpp
        src/Model.cpp
        src/Gui.cpp
        src/DirectionalLight.cpp
        src/CascadedShadowMap.cpp
        src/Physics.cpp
        src/components/PhysicsComponent.cpp
#        src/StaticBufferGroup.cpp
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
        include/terrain/Terrain.hpp
        include/terrain/HeightmapGenerator.hpp
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
        include/DirectionalLight.hpp
        include/CascadedShadowMap.hpp
        include/BindingProvider.hpp
        include/Physics.hpp
        include/components/PhysicsComponent.hpp
#        include/StaticBufferGroup.hpp
)

target_include_directories(dsl
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>
)

set(HEIGHTMAP_GENERATOR_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/terrain/HeightmapGenerator.slang")
set(HEIGHTMAP_GENERATOR_OUTPUT_HPP "${CMAKE_CURRENT_SOURCE_DIR}/include/terrain/HeightmapGenerator.hpp")
set(HEIGHTMAP_GENERATOR_OUTPUT_CPP "${CMAKE_CURRENT_SOURCE_DIR}/src/terrain/HeightmapGenerator.cpp")
find_program(SLANGC_EXECUTABLE NAMES slangc HINTS "$ENV{VULKAN_SDK}/bin")
if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc compiler not found! Please install the Vulkan SDK or add slangc to your PATH.")
endif()
add_custom_command(
    OUTPUT ${HEIGHTMAP_GENERATOR_OUTPUT_HPP}
    COMMAND ${SLANGC_EXECUTABLE} ${HEIGHTMAP_GENERATOR_SOURCE} -target hpp -o ${HEIGHTMAP_GENERATOR_OUTPUT_HPP}
    DEPENDS ${HEIGHTMAP_GENERATOR_SOURCE}
    VERBATIM
    COMMENT "Transpiling HeightmapGenerator header..."
)
add_custom_command(
    OUTPUT ${HEIGHTMAP_GENERATOR_OUTPUT_CPP}
    COMMAND ${SLANGC_EXECUTABLE} ${HEIGHTMAP_GENERATOR_SOURCE} -target cpp -o ${HEIGHTMAP_GENERATOR_OUTPUT_CPP}
    DEPENDS ${HEIGHTMAP_GENERATOR_SOURCE}
    VERBATIM
    COMMENT "Transpiling HeightmapGenerator source..."
)
add_custom_target(CompileHeightmapGenerator ALL
    DEPENDS
        ${HEIGHTMAP_GENERATOR_OUTPUT_HPP}
        ${HEIGHTMAP_GENERATOR_OUTPUT_CPP}
)
add_dependencies(dsl CompileHeightmapGenerator)