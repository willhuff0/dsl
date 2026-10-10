#pragma once

#if !defined(NDEBUG)
#define SRC_BASEPATH "../src"
#define RESOURCE_BASEPATH "../res"
#else
#define SRC_BASEPATH "./src"
#define RESOURCE_BASEPATH "./res"
#endif

#define SHADER_SRC_BASEPATH SRC_BASEPATH "/shaders"
#define SHADER_RES_BASEPATH RESOURCE_BASEPATH "/shaders"

#define SKYBOX_HDRI_PATH RESOURCE_BASEPATH "/skybox.hdr"

#define SB_CUBE_MAP_CONVERTER_VERTEX_SHADER_PATH SHADER_RES_BASEPATH "/skybox/CubeMapConverterVertex.spv"
#define SB_EQUIRECTANGULAR_TO_CUBE_MAP_SHADER_PATH SHADER_RES_BASEPATH "/skybox/EquirectangularToCubeMap.spv"
#define SB_IRRADIANCE_SHADER_PATH SHADER_RES_BASEPATH "/skybox/Irradiance.spv"
#define SB_RADIANCE_SHADER_PATH SHADER_RES_BASEPATH "/skybox/Radiance.spv"
#define SB_ENV_BRDF_LUT_GENERATOR_SHADER_PATH SHADER_RES_BASEPATH "/skybox/EnvBrdfLutGenerator.spv"
#define SB_SKYBOX_SHADER_PATH "/skybox/Skybox"

#define PP_FULLSCREEN_VERTEX_SHADER_PATH SHADER_RES_BASEPATH "/pp/FullscreenVertex.spv"
#define PP_SPLIT_BRIGHTNESS_SHADER_PATH SHADER_RES_BASEPATH "/pp/SplitBrightness.spv"
#define PP_BLUR_SHADER_PATH SHADER_RES_BASEPATH "/pp/Blur.spv"
#define PP_BLIT_SHADER_PATH SHADER_RES_BASEPATH "/pp/Blit.spv"
#define PP_COLOR_CORRECTION_SHADER_PATH SHADER_RES_BASEPATH "/pp/ColorCorrection.spv"

#define TERRAIN_ALBEDO_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/albedo.jpg"
#define TERRAIN_METALLIC_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/metallic.png"
#define TERRAIN_ROUGHNESS_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/roughness.jpg"
#define TERRAIN_AO_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/ao.jpg"

#define TERRAIN_SHADER_PATH "/terrain/Terrain"
#define TERRAIN_NORMALS_SHADER_PATH SHADER_RES_BASEPATH "/terrain/TerrainNormals.spv"

#define TERRAIN_CLIPMAP_MESH_SHADER_PREFIX_PATH RESOURCE_BASEPATH "/terrain/ClipMap_LOD_"
#define TERRAIN_GENERATOR_SHADER_PATH "/terrain/TerrainGenerator"

#define RENDERER_COMPUTE_PASS1_SHADER_PATH "/renderer/Pass1_CullInstances"
#define RENDERER_COMPUTE_PASS2_SHADER_PATH "/renderer/Pass2_CompactCommands"

#define MODEL_SHADER_PATH "/model/Model"

#define GUI_FONT_PATH RESOURCE_BASEPATH "/fonts/RobotoMono.ttf"