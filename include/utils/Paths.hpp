#pragma once

#if !defined(NDEBUG)
#define RESOURCE_BASEPATH "../res"
#else
#define RESOURCE_BASEPATH "./res"
#endif

#define SKYBOX_HDRI_PATH RESOURCE_BASEPATH "/skybox.hdr"

#define SB_CUBE_MAP_CONVERTER_VERTEX_SHADER_PATH RESOURCE_BASEPATH "/shaders/skybox/CubeMapConverterVertex.spv"
#define SB_EQUIRECTANGULAR_TO_CUBE_MAP_SHADER_PATH RESOURCE_BASEPATH "/shaders/skybox/EquirectangularToCubeMap.spv"
#define SB_IRRADIANCE_SHADER_PATH RESOURCE_BASEPATH "/shaders/skybox/Irradiance.spv"
#define SB_RADIANCE_SHADER_PATH RESOURCE_BASEPATH "/shaders/skybox/Radiance.spv"
#define SB_ENV_BRDF_LUT_GENERATOR_SHADER_PATH RESOURCE_BASEPATH "/shaders/skybox/EnvBrdfLutGenerator.spv"
#define SB_SKYBOX_SHADER_PATH RESOURCE_BASEPATH "/shaders/skybox/Skybox.spv"

#define TERRAIN_ALBEDO_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/albedo.jpg"
#define TERRAIN_METALLIC_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/metallic.png"
#define TERRAIN_ROUGHNESS_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/roughness.jpg"
#define TERRAIN_AO_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/ao.jpg"

#define TERRAIN_SHADER_PATH RESOURCE_BASEPATH "/shaders/terrain/Terrain.spv"

#define TERRAIN_GENERATOR_SHADER_PATH RESOURCE_BASEPATH "/shaders/terrain/TerrainGenerator.spv"

#define PP_FULLSCREEN_VERTEX_SHADER_PATH RESOURCE_BASEPATH "/shaders/pp/FullscreenVertex.spv"
#define PP_SPLIT_BRIGHTNESS_SHADER_PATH RESOURCE_BASEPATH "/shaders/pp/SplitBrightness.spv"
#define PP_BLUR_SHADER_PATH RESOURCE_BASEPATH "/shaders/pp/Blur.spv"
#define PP_BLIT_SHADER_PATH RESOURCE_BASEPATH "/shaders/pp/Blit.spv"
#define PP_COLOR_CORRECTION_SHADER_PATH RESOURCE_BASEPATH "/shaders/pp/ColorCorrection.spv"
