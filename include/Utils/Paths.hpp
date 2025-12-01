#pragma once

#if !defined(NDEBUG)
#define RESOURCE_BASEPATH "../res"
#else
#define RESOURCE_BASEPATH "./res"
#endif

#define TERRAIN_SHADER_PATHS RESOURCE_BASEPATH "/terrain_vert.glsl", RESOURCE_BASEPATH "/terrain_frag.glsl"
#define TERRAIN_ALBEDO_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/albedo.png"
#define TERRAIN_METALLIC_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/metallic.png"
#define TERRAIN_ROUGHNESS_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/roughness.png"
#define TERRAIN_AO_TEXTURE_PATH RESOURCE_BASEPATH "/terrain/ao.png"