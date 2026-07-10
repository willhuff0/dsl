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


// NOISE (based on FastNoiseLite implementation) --
static const float Sqrt3 = 1.7320508075688772935274463415059;
static const float F2 = 0.5 * (Sqrt3 - 1);
static const float G2 = (3 - Sqrt3) / 6;

static const int PrimeX = 501125321;
static const int PrimeY = 1136930381;

static const float[] gradients = {
    0.130526192220052f, 0.99144486137381f, 0.38268343236509f, 0.923879532511287f, 0.608761429008721f, 0.793353340291235f, 0.793353340291235f, 0.608761429008721f,
    0.923879532511287f, 0.38268343236509f, 0.99144486137381f, 0.130526192220051f, 0.99144486137381f, -0.130526192220051f, 0.923879532511287f, -0.38268343236509f,
    0.793353340291235f, -0.60876142900872f, 0.608761429008721f, -0.793353340291235f, 0.38268343236509f, -0.923879532511287f, 0.130526192220052f, -0.99144486137381f,
    -0.130526192220052f, -0.99144486137381f, -0.38268343236509f, -0.923879532511287f, -0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
    -0.923879532511287f, -0.38268343236509f, -0.99144486137381f, -0.130526192220052f, -0.99144486137381f, 0.130526192220051f, -0.923879532511287f, 0.38268343236509f,
    -0.793353340291235f, 0.608761429008721f, -0.608761429008721f, 0.793353340291235f, -0.38268343236509f, 0.923879532511287f, -0.130526192220052f, 0.99144486137381f,
    0.130526192220052f, 0.99144486137381f, 0.38268343236509f, 0.923879532511287f, 0.608761429008721f, 0.793353340291235f, 0.793353340291235f, 0.608761429008721f,
    0.923879532511287f, 0.38268343236509f, 0.99144486137381f, 0.130526192220051f, 0.99144486137381f, -0.130526192220051f, 0.923879532511287f, -0.38268343236509f,
    0.793353340291235f, -0.60876142900872f, 0.608761429008721f, -0.793353340291235f, 0.38268343236509f, -0.923879532511287f, 0.130526192220052f, -0.99144486137381f,
    -0.130526192220052f, -0.99144486137381f, -0.38268343236509f, -0.923879532511287f, -0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
    -0.923879532511287f, -0.38268343236509f, -0.99144486137381f, -0.130526192220052f, -0.99144486137381f, 0.130526192220051f, -0.923879532511287f, 0.38268343236509f,
    -0.793353340291235f, 0.608761429008721f, -0.608761429008721f, 0.793353340291235f, -0.38268343236509f, 0.923879532511287f, -0.130526192220052f, 0.99144486137381f,
    0.130526192220052f, 0.99144486137381f, 0.38268343236509f, 0.923879532511287f, 0.608761429008721f, 0.793353340291235f, 0.793353340291235f, 0.608761429008721f,
    0.923879532511287f, 0.38268343236509f, 0.99144486137381f, 0.130526192220051f, 0.99144486137381f, -0.130526192220051f, 0.923879532511287f, -0.38268343236509f,
    0.793353340291235f, -0.60876142900872f, 0.608761429008721f, -0.793353340291235f, 0.38268343236509f, -0.923879532511287f, 0.130526192220052f, -0.99144486137381f,
    -0.130526192220052f, -0.99144486137381f, -0.38268343236509f, -0.923879532511287f, -0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
    -0.923879532511287f, -0.38268343236509f, -0.99144486137381f, -0.130526192220052f, -0.99144486137381f, 0.130526192220051f, -0.923879532511287f, 0.38268343236509f,
    -0.793353340291235f, 0.608761429008721f, -0.608761429008721f, 0.793353340291235f, -0.38268343236509f, 0.923879532511287f, -0.130526192220052f, 0.99144486137381f,
    0.130526192220052f, 0.99144486137381f, 0.38268343236509f, 0.923879532511287f, 0.608761429008721f, 0.793353340291235f, 0.793353340291235f, 0.608761429008721f,
    0.923879532511287f, 0.38268343236509f, 0.99144486137381f, 0.130526192220051f, 0.99144486137381f, -0.130526192220051f, 0.923879532511287f, -0.38268343236509f,
    0.793353340291235f, -0.60876142900872f, 0.608761429008721f, -0.793353340291235f, 0.38268343236509f, -0.923879532511287f, 0.130526192220052f, -0.99144486137381f,
    -0.130526192220052f, -0.99144486137381f, -0.38268343236509f, -0.923879532511287f, -0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
    -0.923879532511287f, -0.38268343236509f, -0.99144486137381f, -0.130526192220052f, -0.99144486137381f, 0.130526192220051f, -0.923879532511287f, 0.38268343236509f,
    -0.793353340291235f, 0.608761429008721f, -0.608761429008721f, 0.793353340291235f, -0.38268343236509f, 0.923879532511287f, -0.130526192220052f, 0.99144486137381f,
    0.130526192220052f, 0.99144486137381f, 0.38268343236509f, 0.923879532511287f, 0.608761429008721f, 0.793353340291235f, 0.793353340291235f, 0.608761429008721f,
    0.923879532511287f, 0.38268343236509f, 0.99144486137381f, 0.130526192220051f, 0.99144486137381f, -0.130526192220051f, 0.923879532511287f, -0.38268343236509f,
    0.793353340291235f, -0.60876142900872f, 0.608761429008721f, -0.793353340291235f, 0.38268343236509f, -0.923879532511287f, 0.130526192220052f, -0.99144486137381f,
    -0.130526192220052f, -0.99144486137381f, -0.38268343236509f, -0.923879532511287f, -0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
    -0.923879532511287f, -0.38268343236509f, -0.99144486137381f, -0.130526192220052f, -0.99144486137381f, 0.130526192220051f, -0.923879532511287f, 0.38268343236509f,
    -0.793353340291235f, 0.608761429008721f, -0.608761429008721f, 0.793353340291235f, -0.38268343236509f, 0.923879532511287f, -0.130526192220052f, 0.99144486137381f,
    0.38268343236509f, 0.923879532511287f, 0.923879532511287f, 0.38268343236509f, 0.923879532511287f, -0.38268343236509f, 0.38268343236509f, -0.923879532511287f,
    -0.38268343236509f, -0.923879532511287f, -0.923879532511287f, -0.38268343236509f, -0.923879532511287f, 0.38268343236509f, -0.38268343236509f, 0.923879532511287f,
};

float2 transformNoiseCoordinates(float2 point) {
    point *= FREQUENCY;

    float t = (point.x + point.y) * F2;
    point += t;

    return point;
}

int hash(int seed, int xPrimed, int yPrimed) {
    int v = seed ^ xPrimed ^ yPrimed;

    v *= 0x27d4eb2d;
    return v;
}

float gradCoord(int seed, int xPrimed, int yPrimed, float xd, float yd) {
    int hashV = hash(seed, xPrimed, yPrimed);
    hashV ^= hashV >> 15;
    hashV &= 127 << 1;

    float xg = gradients[hashV];
    float yg = gradients[hashV | 1];

    return xd * xg + yd * yg;
}

float getNoise(int seed, float2 point) {
    int i = (int)floor(point.x);
    int j = (int)floor(point.y);
    float xi = float(point.x - i);
    float yi = float(point.y - j);

    i *= PrimeX;
    j *= PrimeY;
    int i1 = i + PrimeX;
    int j1 = j + PrimeY;

    float t = (xi + yi) * G2;
    float x0 = xi - t;
    float y0 = yi - t;

    float a0 = (2.0 / 3.0) - x0 * x0 - y0 * y0;
    float value = (a0 * a0) * (a0 * a0) * gradCoord(seed, i, j, x0, y0);

    float a1 = (2 * (1 - 2 * G2) * (1 / G2 - 2)) * t + ((-2 * (1 - 2 * G2) * (1 - 2 * G2)) + a0);
    float x1 = x0 - (1 - 2 * G2);
    float y1 = y0 - (1 - 2 * G2);
    value += (a1 * a1) * (a1 * a1) * gradCoord(seed, i1, j1, x1, y1);

    float xmyi = xi - yi;
    if (t > G2) {
        if (xi + xmyi > 1) {
            float x2 = x0 + (float)(3 * G2 - 2);
            float y2 = y0 + (float)(3 * G2 - 1);
            float a2 = (2.0f / 3.0f) - x2 * x2 - y2 * y2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * gradCoord(seed, i + (PrimeX << 1), j + PrimeY, x2, y2);
            }
        }
        else {
            float x2 = x0 + (float)G2;
            float y2 = y0 + (float)(G2 - 1);
            float a2 = (2.0f / 3.0f) - x2 * x2 - y2 * y2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * gradCoord(seed, i, j + PrimeY, x2, y2);
            }
        }

        if (yi - xmyi > 1) {
            float x3 = x0 + (float)(3 * G2 - 1);
            float y3 = y0 + (float)(3 * G2 - 2);
            float a3 = (2.0f / 3.0f) - x3 * x3 - y3 * y3;
            if (a3 > 0) {
                value += (a3 * a3) * (a3 * a3) * gradCoord(seed, i + PrimeX, j + (PrimeY << 1), x3, y3);
            }
        }
        else {
            float x3 = x0 + (float)(G2 - 1);
            float y3 = y0 + (float)G2;
            float a3 = (2.0f / 3.0f) - x3 * x3 - y3 * y3;
            if (a3 > 0) {
                value += (a3 * a3) * (a3 * a3) * gradCoord(seed, i + PrimeX, j, x3, y3);
            }
        }
    }
    else {
        if (xi + xmyi < 0) {
            float x2 = x0 + (float)(1 - G2);
            float y2 = y0 - (float)G2;
            float a2 = (2.0f / 3.0f) - x2 * x2 - y2 * y2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * gradCoord(seed, i - PrimeX, j, x2, y2);
            }
        }
        else {
            float x2 = x0 + (float)(G2 - 1);
            float y2 = y0 + (float)G2;
            float a2 = (2.0f / 3.0f) - x2 * x2 - y2 * y2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * gradCoord(seed, i + PrimeX, j, x2, y2);
            }
        }

        if (yi < xmyi) {
            float x2 = x0 - (float)G2;
            float y2 = y0 - (float)(G2 - 1);
            float a2 = (2.0f / 3.0f) - x2 * x2 - y2 * y2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * gradCoord(seed, i, j - PrimeY, x2, y2);
            }
        }
        else {
            float x2 = x0 + (float)G2;
            float y2 = y0 + (float)(G2 - 1);
            float a2 = (2.0f / 3.0f) - x2 * x2 - y2 * y2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * gradCoord(seed, i, j + PrimeY, x2, y2);
            }
        }
    }

    return value * 18.24196194486065;
}

// Jagged, ridged multifractal - the original terrain shape. Good for mountains.
float ridged(float2 point) {
    int seed = SEED;
    float sum = 0;
    float amp = AMP;

    for (int i = 0; i < OCTAVES; i++) {
        float noise = abs(getNoise(seed++, point));
        sum += (noise * -2 + 1) * amp;
        amp *= lerp(1.0, 1 - noise, WEIGHTED_STRENGTH);

        point.x *= LACUNARITY;
        point.y *= LACUNARITY;
        amp *= GAIN;
    }

    return sum;
}

// Smooth fractal Brownian motion - rolling hills and plains.
float fbm(float2 point) {
    int seed = SEED;
    float sum = 0;
    float amp = AMP;

    for (int i = 0; i < OCTAVES; i++) {
        sum += getNoise(seed++, point) * amp;

        point.x *= LACUNARITY;
        point.y *= LACUNARITY;
        amp *= GAIN;
    }

    return sum;
}

// Layer 4 - displace the sample point by a low-frequency noise field.
float2 warpPoint(float2 point) {
    float2 offset = float2(
        getNoise(SEED + WARP_SEED,     point * WARP_FREQUENCY),
        getNoise(SEED + WARP_SEED + 1, (point + 5.2) * WARP_FREQUENCY));
    return point + offset * WARP_STRENGTH;
}

// Maps the continent control field to a base elevation with a flat coastal
// shelf, an ocean basin below the coastline, and a steepening rise inland.
float continentCurve(float continent) {
    float t = continent - CONTINENT_OCEAN_LEVEL;
    if (t < 0.0) {
        return t * CONTINENT_OCEAN_DEPTH;
    }
    float landT = t / max(1.0 - CONTINENT_OCEAN_LEVEL, 1e-5);
    return pow(saturate(landT), CONTINENT_POWER);
}

float getHeight(float2 point) {
    point = transformNoiseCoordinates(point);
    point = warpPoint(point);

    // Layer 1 - low-frequency control fields.
    float continent = getNoise(SEED + CONTINENT_SEED, point * CONTINENT_FREQUENCY);
    float erosion   = getNoise(SEED + EROSION_SEED,   point * EROSION_FREQUENCY);

    // Layer 3 - blend terrain archetypes based on erosion.
    float plains    = fbm(point) * PLAINS_AMP;
    float mountains = ridged(point) * MOUNTAIN_AMP;
    float mountainMask = smoothstep(EROSION_MASK_LOW, EROSION_MASK_HIGH, erosion);
    float detail = lerp(plains, mountains, mountainMask);

    // Base continent elevation establishes distinct regions (coast, lowland, highland).
    float base = continentCurve(continent) * CONTINENT_HEIGHT;

    return (base + detail) * HEIGHT_MULTIPLIER;
}
// -- NOISE

[shader("compute")]
[numthreads(32, 32, 1)]
void GeneratePositions(uint3 threadId : SV_DispatchThreadID) {
    int2 local = int2(threadId.xy);
    if (any(local >= u_region.size)) return;

    int2 worldTexel = u_region.worldOrigin + local;
    heightMap[wrapTexel(worldTexel)] = getHeight((float2)worldTexel);
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