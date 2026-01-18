[[vk::constant_id(1)]] const float FREQUENCY = 0.0015;
[[vk::constant_id(2)]] const int SEED = 6767;
[[vk::constant_id(3)]] const float AMP = 0.507936537;
[[vk::constant_id(4)]] const int OCTAVES = 6;
[[vk::constant_id(5)]] const float WEIGHTED_STRENGTH = 0.0;
[[vk::constant_id(6)]] const float LACUNARITY = 2.0;
[[vk::constant_id(7)]] const float GAIN = 0.5;

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

float generateFractal(float2 point) {
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

float getHeight(float2 point) {
    point = transformNoiseCoordinates(point);
    return generateFractal(point) * 24.0;
}
// -- NOISE

[shader("compute")]
[numthreads(32, 32, 1)]
void GeneratePositions(uint3 threadId : SV_DispatchThreadID) {
    float2 uv = (float2)threadId.xy;
    heightMap[threadId.xy] = getHeight(uv);
}

// Normals
[[vk::binding(1, 0)]] RWTexture2D<float4> normalMap;

[[vk::constant_id(0)]] const float METERS_PER_TEXEL = 1.0;

[shader("compute")]
[numthreads(32, 32, 1)]
void GenerateNormals(uint3 threadId: SV_DispatchThreadID) {
    uint width, height;
    heightMap.GetDimensions(width, height);

    if (threadId.x >= width || threadId.y >= height) return;

    int2 pos = int2(threadId.xy);

    float hL = heightMap[int2(max(pos.x - 1, 0), pos.y)];
    float hR = heightMap[int2(min(pos.x + 1, (int)width - 1), pos.y)];
    float hD = heightMap[int2(pos.x, max(pos.y - 1, 0))];
    float hU = heightMap[int2(pos.x, min(pos.y + 1, (int)height - 1))];

    float3 normal;
    normal.x = hL - hR;
    normal.z = hD - hU;
    normal.y = 2.0 * METERS_PER_TEXEL;

    normalMap[pos] = float4(normalize(normal), 0.0);
}