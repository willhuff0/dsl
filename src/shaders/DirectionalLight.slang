static const float SHADOW_NORMAL_BIAS = 0.002;
static const float SHADOW_MIN_BIAS = 0.0005;
static const float SHADOW_MAX_BIAS = 0.005;

static const float CASCADE_BLEND_FRACTION = 0.1;

static const bool SHOW_CASCADES = false;

struct DirectionalLightUniforms {
    float4x4 lightSpaceViewProjMats[4];
    float4 cascadeDistances;
    float3 direction;
    float3 color;
    float cameraZFar;
    uint32_t cascadeCount;
    uint32_t shadowMapSize;
}

float blockSum(float4 g, float2 wx, float2 wy) {
    return g.w * wx.x * wy.x + g.z * wx.y * wy.x
         + g.x * wx.x * wy.y + g.y * wx.y * wy.y;
}

float3 sampleShadowCascade(
    DirectionalLightUniforms directionalLight,
    DepthTexture2DArray cascadedShadowMap,
    SamplerComparisonState cascadedShadowMapSampler,
    int cascadeIndex,
    float3 position,
    float3 normal,
    float3 lightDir
) {
    float cascadeDistance = directionalLight.cascadeDistances[cascadeIndex];

    float3 offsetPos = position + normal * (SHADOW_NORMAL_BIAS * cascadeDistance);

    float4 lightClip = mul(
        directionalLight.lightSpaceViewProjMats[cascadeIndex],
        float4(offsetPos, 1.0)
    );

    float3 ndc = lightClip.xyz / lightClip.w;
    float2 uv = float2(
        ndc.x * 0.5 + 0.5,
        0.5 - ndc.y * 0.5
    );

    if (ndc.z < 0.0 || ndc.z > 1.0) return 0.0;

    float slopeBias = SHADOW_MAX_BIAS * (1.0 - dot(normal, lightDir));
    float bias = clamp(slopeBias, SHADOW_MIN_BIAS, SHADOW_MAX_BIAS) / cascadeDistance;

    float2 texelSize = 1.0 / float2(directionalLight.shadowMapSize);

    // float shadow = 0.0;
    // for (int x = -1; x <= 1; x++) {
    //     for (int y = -1; y <= 1; y++) {
    //         float2 sampleUV = uv + float2(x, y) * texelSize;

    //         float visibility = cascadedShadowMap.SampleCmpLevelZero(
    //             cascadedShadowMapSampler,
    //             float3(sampleUV, float(cascadeIndex)),
    //             ndc.z - bias
    //         );

    //         shadow += 1.0 - visibility;
    //     }
    // }
    // shadow /= 9.0;

    float2 texelPos = uv * float(directionalLight.shadowMapSize) - 0.5;
    float2 base = floor(texelPos);
    float2 f = texelPos - base;

    // Center of the 2x2 block {base, base+1}; this is a texel *corner*, so the
    // hardware's floor() is 0.5 texel from any rounding boundary.
    float2 c = (base + 1.0) * texelSize;
    float z = ndc.z - bias;

    float4 g00 = cascadedShadowMap.GatherCmp(cascadedShadowMapSampler, float3(c + float2(-1, -1) * texelSize, cascadeIndex), z);
    float4 g10 = cascadedShadowMap.GatherCmp(cascadedShadowMapSampler, float3(c + float2(1, -1) * texelSize, cascadeIndex), z);
    float4 g01 = cascadedShadowMap.GatherCmp(cascadedShadowMapSampler, float3(c + float2(-1, 1) * texelSize, cascadeIndex), z);
    float4 g11 = cascadedShadowMap.GatherCmp(cascadedShadowMapSampler, float3(c + float2(1, 1) * texelSize, cascadeIndex), z);

    // Per-axis weights for the 4 columns/rows (base-1, base, base+1, base+2)
    float2 wxL = float2(1.0 - f.x, 1.0); // left block  (base-1, base)
    float2 wxR = float2(1.0, f.x);       // right block (base+1, base+2)
    float2 wyT = float2(1.0 - f.y, 1.0); // upper block (rows base-1, base)
    float2 wyB = float2(1.0, f.y);       // lower block (rows base+1, base+2)

    float visibility =
        blockSum(g00, wxL, wyT) + blockSum(g10, wxR, wyT) +
        blockSum(g01, wxL, wyB) + blockSum(g11, wxR, wyB);

    float shadow = 1.0 - visibility / 9.0;

    if (SHOW_CASCADES) {
        if (cascadeIndex == 0) {
            return shadow * float3(1.0, 0.0, 0.0);
        } else if (cascadeIndex == 1) {
            return shadow * float3(1.0, 1.0, 0.0);
        } else if (cascadeIndex == 2) {
            return shadow * float3(0.0, 1.0, 0.0);
        } else if (cascadeIndex == 3) {
            return shadow * float3(0.0, 1.0, 1.0);
        }
    }

    return shadow;
}

float3 CalculateDirectionalLightShadow(
    DirectionalLightUniforms directionalLight,
    DepthTexture2DArray cascadedShadowMap,
    SamplerComparisonState cascadedShadowMapSampler,
    float4x4 viewMat,
    float3 position,
    float3 normal,
    float3 lightDir
) {
    float4 viewSpacePos = mul(viewMat, float4(position, 1.0));
    float viewZ = abs(viewSpacePos.z);

    int cascadeIndex = -1;
    for (int i = 0; i < int(min(directionalLight.cascadeCount, 4u)); i++) {
        if (viewZ < directionalLight.cascadeDistances[i]) {
            cascadeIndex = i;
            break;
        }
    }

    if (cascadeIndex == -1) return 0.0;

    float3 shadow = sampleShadowCascade(
        directionalLight,
        cascadedShadowMap,
        cascadedShadowMapSampler,
        cascadeIndex,
        position,
        normal,
        lightDir
    );

    if (cascadeIndex < directionalLight.cascadeCount - 1) {
        float splitNear = cascadeIndex == 0
                              ? 0.0
                              : directionalLight.cascadeDistances[cascadeIndex - 1];

        float splitFar = directionalLight.cascadeDistances[cascadeIndex];
        float blendWidth = (splitFar - splitNear) * CASCADE_BLEND_FRACTION;
        float blendStart = splitFar - blendWidth;

        if (viewZ > blendStart && blendWidth > 0.0) {
            float blendFactor = smoothstep(
                blendStart,
                splitFar,
                viewZ
            );

            float3 nextShadow = sampleShadowCascade(
                directionalLight,
                cascadedShadowMap,
                cascadedShadowMapSampler,
                cascadeIndex + 1,
                position,
                normal,
                lightDir
            );

            shadow = lerp(shadow, nextShadow, blendFactor);
        }
    }

    return shadow;
}