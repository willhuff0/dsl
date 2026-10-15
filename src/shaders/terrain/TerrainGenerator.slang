#include "HeightmapGenerator.slang"

[[vk::constant_id(1)]] const float FREQUENCY = 0.001;
[[vk::constant_id(2)]] const int SEED = 6767;
[[vk::constant_id(3)]] const float AMP = 0.507936537;
[[vk::constant_id(4)]] const int OCTAVES = 10;
[[vk::constant_id(5)]] const float WEIGHTED_STRENGTH = 0.0;
[[vk::constant_id(6)]] const float LACUNARITY = 2.0;
[[vk::constant_id(7)]] const float GAIN = 0.4;
[[vk::constant_id(8)]] const float HEIGHT_MULTIPLIER = 100.0;

// Layer 4 - domain warping. Bends ridgelines/valleys into natural, non-grid shapes.
[[vk::constant_id(9)]]  const float WARP_FREQUENCY = 0.5;   // frequency of the warp field, relative to the base point scale
[[vk::constant_id(10)]] const float WARP_STRENGTH = 0.25;   // how far coordinates are displaced, in post-FREQUENCY units (0 disables warping)
[[vk::constant_id(11)]] const int   WARP_SEED = 700;        // seed offset for the warp field

// Layer 1 - low-frequency control fields that decide what kind of terrain forms where.
[[vk::constant_id(12)]] const float CONTINENT_FREQUENCY = 0.15; // relative frequency of the continent field
[[vk::constant_id(13)]] const int   CONTINENT_SEED = 100;       // seed offset for the continent field
[[vk::constant_id(14)]] const float EROSION_FREQUENCY = 0.35;   // relative frequency of the erosion field
[[vk::constant_id(15)]] const int   EROSION_SEED = 200;         // seed offset for the erosion field

// Layer 3 - terrain archetype amplitudes and how erosion selects between them.
[[vk::constant_id(16)]] const float PLAINS_AMP = 0.3;        // amplitude of the smooth (FBM) plains contribution
[[vk::constant_id(17)]] const float MOUNTAIN_AMP = 1.0;      // amplitude of the jagged (ridged) mountain contribution
[[vk::constant_id(18)]] const float EROSION_MASK_LOW = 0.1;  // erosion value at which terrain is fully plains
[[vk::constant_id(19)]] const float EROSION_MASK_HIGH = -0.4;// erosion value at which terrain is fully mountains

// Continent shaping curve - maps the continent field to a base elevation.
[[vk::constant_id(20)]] const float CONTINENT_HEIGHT = 4.0;     // scale of the base continent elevation
[[vk::constant_id(21)]] const float CONTINENT_OCEAN_LEVEL = -0.2; // continent value below which ocean basins form
[[vk::constant_id(22)]] const float CONTINENT_OCEAN_DEPTH = 0.6;  // how far below zero ocean basins sink
[[vk::constant_id(23)]] const float CONTINENT_POWER = 1.5;        // >1 keeps lowlands flat and steepens highlands

// Toroidal streaming - the heightmap is a wrapping window that slides with the camera.
[[vk::constant_id(24)]] const uint HEIGHTMAP_SIZE = 4096;

// Region of world-texel space to (re)generate this dispatch.
struct GenRegion {
    int2 worldOrigin; // world-texel coordinate of the region's (0,0) thread
    int2 size;        // region extent in texels
};
[vk::push_constant] ConstantBuffer<GenRegion> u_region;

int2 wrapTexel(int2 p) {
    int size = (int)HEIGHTMAP_SIZE;
    return ((p % size) + size) % size;
}

// Positions
[[vk::binding(0, 0)]] RWTexture2D<float> heightMap;

[shader("compute")]
[numthreads(32, 32, 1)]
void GeneratePositions(uint3 threadId : SV_DispatchThreadID) {
    int2 local = int2(threadId.xy);
    if (any(local >= u_region.size)) return;

    GenConfig config;
    config.frequency = FREQUENCY;
    config.seed = SEED;
    config.amp = AMP;
    config.octaves = OCTAVES;
    config.weightedStrength = WEIGHTED_STRENGTH;
    config.lacunarity = LACUNARITY;
    config.gain = GAIN;
    config.heightMultiplier = HEIGHT_MULTIPLIER;
    config.warpFrequency = WARP_FREQUENCY;
    config.warpStrength = WARP_STRENGTH;
    config.warpSeed = WARP_SEED;
    config.continentFrequency = CONTINENT_FREQUENCY;
    config.continentSeed = CONTINENT_SEED;
    config.erosionFrequency = EROSION_FREQUENCY;
    config.erosionSeed = EROSION_SEED;
    config.plainsAmp = PLAINS_AMP;
    config.mountainAmp = MOUNTAIN_AMP;
    config.erosionMaskLow = EROSION_MASK_LOW;
    config.erosionMaskHigh = EROSION_MASK_HIGH;
    config.continentHeight = CONTINENT_HEIGHT;
    config.continentOceanLevel = CONTINENT_OCEAN_LEVEL;
    config.continentOceanDepth = CONTINENT_OCEAN_DEPTH;
    config.continentPower = CONTINENT_POWER;

    int2 worldTexel = u_region.worldOrigin + local;
    heightMap[wrapTexel(worldTexel)] = getHeight(config, (float2)worldTexel);
}

// Normals
[[vk::binding(1, 0)]] RWTexture2D<float4> normalMap;

[[vk::constant_id(0)]] const float METERS_PER_TEXEL = 1.0;

[shader("compute")]
[numthreads(32, 32, 1)]
void GenerateNormals(uint3 threadId: SV_DispatchThreadID) {
    int2 local = int2(threadId.xy);
    if (any(local >= u_region.size)) return;

    int2 worldTexel = u_region.worldOrigin + local;
    int2 pos = wrapTexel(worldTexel);

    float hL = heightMap[wrapTexel(worldTexel + int2(-1, 0))];
    float hR = heightMap[wrapTexel(worldTexel + int2( 1, 0))];
    float hD = heightMap[wrapTexel(worldTexel + int2( 0, -1))];
    float hU = heightMap[wrapTexel(worldTexel + int2( 0, 1))];

    float3 normal;
    normal.x = hL - hR;
    normal.z = hD - hU;
    normal.y = 2.0 * METERS_PER_TEXEL;

    normalMap[pos] = float4(normalize(normal), 0.0);
}