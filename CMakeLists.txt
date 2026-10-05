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
find_package(nlohmann_json CONFIG REQUIRED)
find_package(assimp REQUIRED)
target_link_libraries(zombies
    PRIVATE
        glfw
        dvk
        assimp::assimp
        nlohmann_json::nlohmann_json
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
        src/Skybox.cpp
#        src/Node.cpp
        src/PostProcessing.cpp
        src/Vehicle.cpp
        src/Terrain.cpp
        src/TempModelLoader.cpp
        src/HotReload.cpp
        src/utils/InputStream.cpp
        src/utils/GrowingBuffer.cpp
        src/Gltf.cpp
        src/Renderer.cpp
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
        include/Skybox.hpp
        include/PostProcessing.hpp
#        include/Node.hpp
        include/Vehicle.hpp
        include/Terrain.hpp
        include/TempModelLoader.hpp
        include/HotReload.hpp
        include/utils/InputStream.hpp
        include/utils/Serialization.hpp
        include/utils/GrowingBuffer.hpp
        include/Gltf.hpp
        include/Renderer.hpp
)

target_include_directories(zombies
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>
)

## Function to compile Slang shaders and attach them to a target
## Usage: target_compile_slang_shaders(<target_name> SOURCES <shader1> <shader2> ... [BASE_DIR <dir>])
#function(target_compile_slang_shaders TARGET_NAME)
#    set(options)
#    set(oneValueArgs BASE_DIR OUTPUT_DIR)
#    set(multiValueArgs SOURCES)
#    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
#
#    if(NOT ARG_BASE_DIR)
#        set(ARG_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
#    endif()
#
#    if(NOT ARG_OUTPUT_DIR)
#        set(ARG_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res/shaders")
#    endif()
#
#    set(SPV_FILES)
#
#    foreach(SLANG_FILE ${ARG_SOURCES})
#        # Calculate relative path to maintain structure
#        file(RELATIVE_PATH REL_PATH "${ARG_BASE_DIR}" "${SLANG_FILE}")
#
#        # Determine output filename
#        get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)
#        get_filename_component(FILE_NAME "${REL_PATH}" NAME_WE)
#
#        set(OUT_DIR "${ARG_OUTPUT_DIR}/${REL_DIR}")
#        set(OUT_FILE "${OUT_DIR}/${FILE_NAME}.spv")
#
#        # Create output directory if it doesn't exist
#        file(MAKE_DIRECTORY "${OUT_DIR}")
#
#        # Add command to compile the shader
#        add_custom_command(
#                OUTPUT "${OUT_FILE}"
#                COMMAND slangc "${SLANG_FILE}"
#                -target spirv
#                -profile spirv_1_4
#                -emit-spirv-directly
#                -fvk-use-entrypoint-name
#                -o "${OUT_FILE}"
#                DEPENDS "${SLANG_FILE}"
#                COMMENT "Compiling Slang shader: ${SLANG_FILE}"
#                VERBATIM
#        )
#
#        list(APPEND SPV_FILES "${OUT_FILE}")
#    endforeach()
#
#    # Create a custom target for the shaders
#    set(SHADER_TARGET "${TARGET_NAME}_shaders")
#    add_custom_target(${SHADER_TARGET} DEPENDS ${SPV_FILES})
#
#    # Make the main target depend on the shader compilation
#    add_dependencies(${TARGET_NAME} ${SHADER_TARGET})
#endfunction()
#
## Find all slang files
#file(GLOB SHADER_SOURCES
#    "src/shaders/pp/FullscreenVertex.slang"
#    "src/shaders/pp/ColorCorrection.slang"
#    "src/shaders/pp/FullscreenVertex.slang"
#    "src/shaders/pp/SplitBrightness.slang"
#    "src/shaders/terrain/Terrain.slang"
#)
#
## Call the function
#target_compile_slang_shaders(zombies
#    SOURCES ${SHADER_SOURCES}
#    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders"
#    OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res/shaders"
#)