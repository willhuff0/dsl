#pragma once

#if !defined(NDEBUG)
#define RESOURCE_BASEPATH "../res"
#else
#define RESOURCE_BASEPATH "./res"
#endif

#define TERRAIN_SHADER_PATHS RESOURCE_BASEPATH "/terrain_vert.glsl", RESOURCE_BASEPATH "/terrain_frag.glsl"