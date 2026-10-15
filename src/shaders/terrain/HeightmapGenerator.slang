export __extern_cpp struct GenConfig {
    export __extern_cpp float frequency = 0.001;
    export __extern_cpp int seed = 6767;
    export __extern_cpp float amp = 0.507936537;
    export __extern_cpp int octaves = 10;
    export __extern_cpp float weightedStrength = 0.0;
    export __extern_cpp float lacunarity = 2.0;
    export __extern_cpp float gain = 0.4;
    export __extern_cpp float heightMultiplier = 100.0;

    // Layer 4 - domain warping. Bends ridgelines/valleys into natural, non-grid shapes.
    export __extern_cpp float warpFrequency = 0.5; // frequency of the warp field, relative to the base point scale
    export __extern_cpp float warpStrength = 0.25; // how far coordinates are displaced, in post-FREQUENCY units (0 disables warping)
    export __extern_cpp int warpSeed = 700;        // seed offset for the warp field

    // Layer 1 - low-frequency control fields that decide what kind of terrain forms where.
    export __extern_cpp float continentFrequency = 0.15; // relative frequency of the continent field
    export __extern_cpp int continentSeed = 100;         // seed offset for the continent field
    export __extern_cpp float erosionFrequency = 0.35;   // relative frequency of the erosion field
    export __extern_cpp int erosionSeed = 200;         // seed offset for the erosion field
    
    // Layer 3 - terrain archetype amplitudes and how erosion selects between them.
    export __extern_cpp float plainsAmp = 0.3;        // amplitude of the smooth (FBM) plains contribution
    export __extern_cpp float mountainAmp = 1.0;      // amplitude of the jagged (ridged) mountain contribution
    export __extern_cpp float erosionMaskLow = 0.1;  // erosion value at which terrain is fully plains
    export __extern_cpp float erosionMaskHigh = -0.4;// erosion value at which terrain is fully mountains
    
    // Continent shaping curve - maps the continent field to a base elevation.
    export __extern_cpp float continentHeight = 4.0;     // scale of the base continent elevation
    export __extern_cpp float continentOceanLevel = -0.2; // continent value below which ocean basins form
    export __extern_cpp float continentOceanDepth = 0.6;  // how far below zero ocean basins sink
    export __extern_cpp float continentPower = 1.5;        // >1 keeps lowlands flat and steepens highlands
};

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

float2 transformNoiseCoordinates(GenConfig config, float2 point) {
    point *= config.frequency;

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
float ridged(GenConfig config, float2 point) {
    int seed = config.seed;
    float sum = 0;
    float amp = config.amp;

    for (int i = 0; i < config.octaves; i++) {
        float noise = abs(getNoise(seed++, point));
        sum += (noise * -2 + 1) * amp;
        amp *= lerp(1.0, 1 - noise, config.weightedStrength);

        point.x *= config.lacunarity;
        point.y *= config.lacunarity;
        amp *= config.gain;
    }

    return sum;
}

// Smooth fractal Brownian motion - rolling hills and plains.
float fbm(GenConfig config, float2 point) {
    int seed = config.seed;
    float sum = 0;
    float amp = config.amp;

    for (int i = 0; i < config.octaves; i++) {
        sum += getNoise(seed++, point) * amp;

        point.x *= config.lacunarity;
        point.y *= config.lacunarity;
        amp *= config.gain;
    }

    return sum;
}

// Layer 4 - displace the sample point by a low-frequency noise field.
float2 warpPoint(GenConfig config, float2 point) {
    float2 offset = float2(
        getNoise(config.seed + config.warpSeed, point * config.warpFrequency),
        getNoise(config.seed + config.warpSeed + 1, (point + 5.2) * config.warpFrequency));
    return point + offset * config.warpStrength;
}

// Maps the continent control field to a base elevation with a flat coastal
// shelf, an ocean basin below the coastline, and a steepening rise inland.
float continentCurve(GenConfig config, float continent) {
    float t = continent - config.continentOceanLevel;
    if (t < 0.0) {
        return t * config.continentOceanDepth;
    }
    float landT = t / max(1.0 - config.continentOceanLevel, 1e-5);
    return pow(saturate(landT), config.continentPower);
}

export __extern_cpp float getHeight(GenConfig config, float2 point) {
    point = transformNoiseCoordinates(config, point);
    point = warpPoint(config, point);

    // Layer 1 - low-frequency control fields.
    float continent = getNoise(config.seed + config.continentSeed, point * config.continentFrequency);
    float erosion = getNoise(config.seed + config.erosionSeed,   point * config.erosionFrequency);

    // Layer 3 - blend terrain archetypes based on erosion.
    float plains = fbm(config, point) * config.plainsAmp;
    float mountains = ridged(config, point) * config.mountainAmp;
    float mountainMask = smoothstep(config.erosionMaskLow, config.erosionMaskHigh, erosion);
    float detail = lerp(plains, mountains, mountainMask);

    // Base continent elevation establishes distinct regions (coast, lowland, highland).
    float base = continentCurve(config, continent) * config.continentHeight;

    return (base + detail) * config.heightMultiplier;
}